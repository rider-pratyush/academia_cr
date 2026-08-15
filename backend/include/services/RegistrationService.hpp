#pragma once
/**
 * RegistrationService.hpp — Concurrent-safe course registration
 * 
 * OS CONCEPTS DEMONSTRATED:
 * ─────────────────────────
 * 
 * RACE CONDITION PREVENTION (THE CORE CONCURRENCY CHALLENGE):
 * 
 * Consider this scenario:
 *   Course CS301 has capacity=40, enrolled=39 (1 seat left)
 *   Thread A: Student Alice checks → 1 seat available → proceeds to register
 *   Thread B: Student Bob checks → 1 seat available → proceeds to register
 *   Both succeed → enrolled=41 → OVERBOOKING!
 * 
 * This is a classic TOCTOU (Time-of-Check-to-Time-of-Use) race condition.
 * 
 * SOLUTION: THREE LAYERS OF PROTECTION
 * 
 * 1. APPLICATION-LEVEL MUTEX (std::mutex per course):
 *    - Each course has its own mutex in a map
 *    - Only one thread can register for a specific course at a time
 *    - Different courses can be registered concurrently (fine-grained locking)
 *    - Uses std::shared_mutex on the map itself to allow concurrent reads
 * 
 * 2. DATABASE TRANSACTION (BEGIN IMMEDIATE):
 *    - Acquires a RESERVED lock on the SQLite database file
 *    - The check-and-insert happens atomically within the transaction
 *    - Even if the app-level mutex were bypassed (e.g., multiple server instances),
 *      the database transaction prevents double-enrollment
 * 
 * 3. UNIQUE CONSTRAINT (student_id, course_id):
 *    - Database-level constraint prevents duplicate registrations
 *    - This is the last line of defense — even if both above layers fail,
 *      the database itself rejects the duplicate
 * 
 * WHY ALL THREE?
 *   - Mutex: Performance (avoids unnecessary DB transaction attempts)
 *   - Transaction: Correctness across potential multi-instance deployments
 *   - Unique constraint: Defense-in-depth (belt AND suspenders)
 */

#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "database/Database.hpp"
#include "models/User.hpp"
#include "utils/Logger.hpp"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <string>

namespace academia {

class RegistrationService {
public:
    RegistrationService(Database& db, CourseRepository& course_repo, 
                        RegistrationRepository& reg_repo)
        : db_(db), course_repo_(course_repo), reg_repo_(reg_repo) {}

    struct RegistrationResult {
        bool success = false;
        int64_t registration_id = 0;
        std::string error;
        int remaining_seats = 0;
    };

    /**
     * Register a student for a course — THREAD-SAFE.
     * 
     * Algorithm:
     * 1. Acquire per-course mutex (ensures only one thread per course)
     * 2. Begin IMMEDIATE transaction (acquires DB write lock)
     * 3. Check: course exists?
     * 4. Check: student already enrolled?
     * 5. Check: seats available? (enrolled_count < capacity)
     * 6. Insert registration
     * 7. Commit transaction
     * 8. Release mutex
     * 
     * If any check fails → rollback + return error.
     * If any exception → rollback (via RAII transaction wrapper) + return error.
     */
    RegistrationResult register_student(int64_t student_id, int64_t course_id) {
        // Step 1: Acquire the per-course mutex
        // This prevents two threads from simultaneously registering for the same course.
        // We use a shared_mutex on the map to allow concurrent access to DIFFERENT courses.
        auto& course_mutex = get_course_mutex(course_id);
        std::lock_guard<std::mutex> course_lock(course_mutex);

        LOG_INFO("Registration attempt: student=" + std::to_string(student_id) + 
                 " course=" + std::to_string(course_id));

        try {
            // Step 2-7: Everything inside a transaction
            return db_.transaction([&]() -> RegistrationResult {
                // Step 3: Check course exists
                auto course = course_repo_.find_by_id(course_id);
                if (!course) {
                    return {false, 0, "Course not found", 0};
                }

                // Step 4: Check if already enrolled
                if (reg_repo_.exists(student_id, course_id)) {
                    return {false, 0, "Already enrolled in this course", 
                            course->capacity - course->enrolled_count};
                }

                // Step 5: Check capacity
                // CRITICAL: We read enrolled_count INSIDE the transaction.
                // Because we used BEGIN IMMEDIATE, no other writer can modify
                // the registrations table concurrently.
                int enrolled = course_repo_.get_enrolled_count(course_id);
                int remaining = course->capacity - enrolled;

                if (remaining <= 0) {
                    LOG_WARN("Registration failed: course full - course=" + 
                             std::to_string(course_id));
                    return {false, 0, "Course is full", 0};
                }

                // Step 6: Insert registration
                int64_t reg_id = reg_repo_.create(student_id, course_id);

                LOG_INFO("Registration successful: student=" + std::to_string(student_id) + 
                         " course=" + course->course_code + 
                         " remaining_seats=" + std::to_string(remaining - 1));

                return {true, reg_id, "", remaining - 1};
            });
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Registration error: ") + e.what());
            return {false, 0, "Registration failed: " + std::string(e.what()), 0};
        }
    }

    /**
     * Drop a student from a course.
     * Also uses per-course mutex to prevent race with concurrent registrations.
     */
    RegistrationResult drop_student(int64_t student_id, int64_t course_id) {
        auto& course_mutex = get_course_mutex(course_id);
        std::lock_guard<std::mutex> course_lock(course_mutex);

        LOG_INFO("Drop attempt: student=" + std::to_string(student_id) + 
                 " course=" + std::to_string(course_id));

        try {
            return db_.transaction([&]() -> RegistrationResult {
                if (!reg_repo_.exists(student_id, course_id)) {
                    return {false, 0, "Not enrolled in this course", 0};
                }

                reg_repo_.drop_registration(student_id, course_id);

                auto course = course_repo_.find_by_id(course_id);
                int remaining = course ? (course->capacity - course->enrolled_count + 1) : 0;

                LOG_INFO("Drop successful: student=" + std::to_string(student_id) + 
                         " course=" + std::to_string(course_id));

                return {true, 0, "", remaining};
            });
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Drop error: ") + e.what());
            return {false, 0, "Drop failed: " + std::string(e.what()), 0};
        }
    }

    /** Get all registrations for a student. */
    std::vector<Registration> get_student_registrations(int64_t student_id) {
        return reg_repo_.find_by_student(student_id);
    }

    /** Get all students enrolled in a course. */
    std::vector<Registration> get_course_enrollments(int64_t course_id) {
        return reg_repo_.find_by_course(course_id);
    }

    /** Get registration statistics. */
    nlohmann::json get_statistics() {
        return {
            {"total_active_registrations", reg_repo_.count_active()},
        };
    }

private:
    /**
     * Get or create a mutex for the given course.
     * 
     * Uses a READER-WRITER LOCK (std::shared_mutex) on the map:
     *   - Shared lock to READ an existing mutex (most common case)
     *   - Exclusive lock to INSERT a new mutex (rare, first-time access)
     * 
     * This is more efficient than a single global mutex because:
     *   - Registrations for DIFFERENT courses can proceed in parallel
     *   - Only registrations for the SAME course are serialized
     */
    std::mutex& get_course_mutex(int64_t course_id) {
        // Try shared (read) lock first — most common case
        {
            std::shared_lock<std::shared_mutex> read_lock(map_mutex_);
            auto it = course_mutexes_.find(course_id);
            if (it != course_mutexes_.end()) {
                return *it->second;
            }
        }
        // Need exclusive (write) lock to insert
        std::unique_lock<std::shared_mutex> write_lock(map_mutex_);
        // Double-check after acquiring write lock (another thread may have inserted)
        auto [it, inserted] = course_mutexes_.try_emplace(
            course_id, std::make_unique<std::mutex>());
        return *it->second;
    }

    Database& db_;
    CourseRepository& course_repo_;
    RegistrationRepository& reg_repo_;

    // Per-course mutexes for fine-grained locking
    std::shared_mutex map_mutex_;
    std::unordered_map<int64_t, std::unique_ptr<std::mutex>> course_mutexes_;
};

} // namespace academia
