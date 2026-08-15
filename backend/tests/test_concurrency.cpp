/**
 * test_concurrency.cpp — Concurrent registration stress test
 * 
 * THE MOST IMPORTANT TEST IN THIS PROJECT
 * 
 * This test demonstrates that the registration system correctly handles
 * concurrent registrations without overbooking.
 * 
 * SCENARIO:
 *   - 1 course with capacity 20
 *   - 100 students attempt to register simultaneously
 *   - Expected: exactly 20 succeed, 80 fail
 *   - No duplicate registrations
 *   - No database corruption
 * 
 * OS CONCEPTS TESTED:
 *   - Thread safety of RegistrationService
 *   - Mutex correctness under contention
 *   - SQLite transaction isolation
 *   - Race condition prevention
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <barrier>
#include <chrono>

#include "database/Database.hpp"
#include "concurrency/ThreadPool.hpp"
#include "repositories/UserRepository.hpp"
#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "services/RegistrationService.hpp"
#include "services/AuthService.hpp"

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<academia::Database>(":memory:");
        db->initialize_schema();
        user_repo = std::make_unique<academia::UserRepository>(*db);
        course_repo = std::make_unique<academia::CourseRepository>(*db);
        reg_repo = std::make_unique<academia::RegistrationRepository>(*db);
        reg_service = std::make_unique<academia::RegistrationService>(
            *db, *course_repo, *reg_repo);
    }

    int64_t create_faculty() {
        academia::User faculty;
        faculty.username = "prof_concurrent";
        faculty.password_hash = academia::AuthService::hash_password("pass");
        faculty.role = academia::Role::Faculty;
        faculty.name = "Dr. Concurrent";
        return user_repo->create(faculty);
    }

    int64_t create_student(int idx) {
        academia::User student;
        student.username = "concurrent_student_" + std::to_string(idx);
        student.password_hash = academia::AuthService::hash_password("pass");
        student.role = academia::Role::Student;
        student.name = "Student " + std::to_string(idx);
        return user_repo->create(student);
    }

    int64_t create_course(int64_t faculty_id, int capacity) {
        auto stmt = db->prepare(
            "INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity) "
            "VALUES (?, ?, 3, ?, ?)");
        stmt.bind(1, std::string("CONC") + std::to_string(capacity));
        stmt.bind(2, std::string("Concurrent Test Course"));
        stmt.bind(3, faculty_id);
        stmt.bind(4, capacity);
        stmt.step();
        return db->last_insert_id();
    }

    std::unique_ptr<academia::Database> db;
    std::unique_ptr<academia::UserRepository> user_repo;
    std::unique_ptr<academia::CourseRepository> course_repo;
    std::unique_ptr<academia::RegistrationRepository> reg_repo;
    std::unique_ptr<academia::RegistrationService> reg_service;
};

/**
 * TEST: 100 threads race to register for a course with capacity 20.
 * 
 * This is the definitive test for race-condition prevention.
 * 
 * Without proper synchronization, possible failures:
 *   - Overbooking: more than 20 students enrolled
 *   - Lost updates: fewer than 20 students enrolled
 *   - Duplicate registrations
 *   - Database corruption / exceptions
 */
TEST_F(ConcurrencyTest, HundredThreadsCapacityTwenty) {
    const int NUM_THREADS = 100;
    const int CAPACITY = 20;

    auto faculty_id = create_faculty();
    auto course_id = create_course(faculty_id, CAPACITY);

    // Create all students first
    std::vector<int64_t> student_ids;
    for (int i = 0; i < NUM_THREADS; ++i) {
        student_ids.push_back(create_student(i));
    }

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    // Use a barrier to synchronize thread start — ensures maximum contention
    // All threads wait at the barrier, then ALL start registering simultaneously
    std::barrier sync_point(NUM_THREADS);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            sync_point.arrive_and_wait();  // All threads start together

            auto result = reg_service->register_student(student_ids[i], course_id);
            if (result.success) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // ASSERTIONS — the core correctness checks
    EXPECT_EQ(success_count.load(), CAPACITY) 
        << "Exactly " << CAPACITY << " registrations should succeed";
    EXPECT_EQ(failure_count.load(), NUM_THREADS - CAPACITY) 
        << "Exactly " << (NUM_THREADS - CAPACITY) << " registrations should fail";
    EXPECT_EQ(success_count.load() + failure_count.load(), NUM_THREADS)
        << "All threads should complete";

    // Verify database state
    auto enrollments = reg_service->get_course_enrollments(course_id);
    EXPECT_EQ(static_cast<int>(enrollments.size()), CAPACITY)
        << "Database should show exactly " << CAPACITY << " active registrations";

    // Verify no duplicates
    std::set<int64_t> enrolled_students;
    for (const auto& reg : enrollments) {
        EXPECT_TRUE(enrolled_students.insert(reg.student_id).second)
            << "Duplicate registration detected for student " << reg.student_id;
    }

    std::cout << "\n=== Concurrency Test Results ===" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Capacity: " << CAPACITY << std::endl;
    std::cout << "Successful: " << success_count.load() << std::endl;
    std::cout << "Failed: " << failure_count.load() << std::endl;
    std::cout << "Duration: " << duration << "ms" << std::endl;
    std::cout << "DB enrollments: " << enrollments.size() << std::endl;
    std::cout << "Unique students: " << enrolled_students.size() << std::endl;
    std::cout << "=== PASSED ===" << std::endl;
}

/**
 * TEST: Two students race for the last seat.
 * 
 * Course with capacity 1. Two threads try to register simultaneously.
 * Exactly one should succeed.
 */
TEST_F(ConcurrencyTest, LastSeatRace) {
    auto faculty_id = create_faculty();

    // Run this test multiple times to increase confidence
    for (int trial = 0; trial < 10; ++trial) {
        auto course_id = create_course(faculty_id, 1);
        auto student_a = create_student(1000 + trial * 2);
        auto student_b = create_student(1000 + trial * 2 + 1);

        std::atomic<int> successes{0};
        std::barrier sync(2);

        std::thread t1([&]() {
            sync.arrive_and_wait();
            if (reg_service->register_student(student_a, course_id).success)
                successes.fetch_add(1);
        });

        std::thread t2([&]() {
            sync.arrive_and_wait();
            if (reg_service->register_student(student_b, course_id).success)
                successes.fetch_add(1);
        });

        t1.join();
        t2.join();

        EXPECT_EQ(successes.load(), 1)
            << "Trial " << trial << ": exactly one student should get the last seat";
    }
}

/**
 * TEST: Concurrent register and drop.
 * 
 * While one thread drops a course, another tries to register.
 * System should remain consistent.
 */
TEST_F(ConcurrencyTest, ConcurrentRegisterAndDrop) {
    auto faculty_id = create_faculty();
    auto course_id = create_course(faculty_id, 1);
    auto student_a = create_student(2000);
    auto student_b = create_student(2001);

    // Student A registers first
    auto result = reg_service->register_student(student_a, course_id);
    ASSERT_TRUE(result.success);

    // Now student A drops and student B registers simultaneously
    std::barrier sync(2);
    bool a_dropped = false;
    bool b_registered = false;

    std::thread t1([&]() {
        sync.arrive_and_wait();
        a_dropped = reg_service->drop_student(student_a, course_id).success;
    });

    std::thread t2([&]() {
        sync.arrive_and_wait();
        b_registered = reg_service->register_student(student_b, course_id).success;
    });

    t1.join();
    t2.join();

    // Student A should always drop successfully
    EXPECT_TRUE(a_dropped);

    // The system should be consistent regardless of ordering
    auto enrollments = reg_service->get_course_enrollments(course_id);
    EXPECT_LE(static_cast<int>(enrollments.size()), 1)
        << "At most 1 student should be enrolled";
}

/**
 * TEST: Thread pool integration.
 * Uses the ThreadPool to submit registration tasks.
 */
TEST_F(ConcurrencyTest, ThreadPoolRegistration) {

    auto faculty_id = create_faculty();
    auto course_id = create_course(faculty_id, 10);

    std::vector<int64_t> students;
    for (int i = 0; i < 50; ++i) {
        students.push_back(create_student(3000 + i));
    }

    academia::ThreadPool pool(8);
    std::vector<std::future<academia::RegistrationService::RegistrationResult>> futures;

    for (int i = 0; i < 50; ++i) {
        futures.push_back(pool.enqueue([&, i]() {
            return reg_service->register_student(students[i], course_id);
        }));
    }

    int successes = 0;
    for (auto& f : futures) {
        if (f.get().success) successes++;
    }

    EXPECT_EQ(successes, 10) << "Exactly 10 should succeed (capacity)";
}
