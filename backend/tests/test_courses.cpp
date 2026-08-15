/**
 * test_courses.cpp — Course service unit tests
 */

#include <gtest/gtest.h>
#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"
#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "services/CourseService.hpp"
#include "services/AuthService.hpp"

class CourseTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<academia::Database>(":memory:");
        db->initialize_schema();
        user_repo = std::make_unique<academia::UserRepository>(*db);
        course_repo = std::make_unique<academia::CourseRepository>(*db);
        reg_repo = std::make_unique<academia::RegistrationRepository>(*db);
        course_service = std::make_unique<academia::CourseService>(*course_repo, *reg_repo);

        // Create faculty user
        academia::User faculty;
        faculty.username = "prof1";
        faculty.password_hash = academia::AuthService::hash_password("pass");
        faculty.role = academia::Role::Faculty;
        faculty.name = "Dr. Test";
        faculty_id = user_repo->create(faculty);
        faculty_user = *user_repo->find_by_id(faculty_id);

        // Create admin user
        academia::User admin;
        admin.username = "admin1";
        admin.password_hash = academia::AuthService::hash_password("pass");
        admin.role = academia::Role::Admin;
        admin.name = "Admin";
        admin_id = user_repo->create(admin);
        admin_user = *user_repo->find_by_id(admin_id);

        // Create student user
        academia::User student;
        student.username = "student1";
        student.password_hash = academia::AuthService::hash_password("pass");
        student.role = academia::Role::Student;
        student.name = "Student";
        student_id = user_repo->create(student);
        student_user = *user_repo->find_by_id(student_id);
    }

    std::unique_ptr<academia::Database> db;
    std::unique_ptr<academia::UserRepository> user_repo;
    std::unique_ptr<academia::CourseRepository> course_repo;
    std::unique_ptr<academia::RegistrationRepository> reg_repo;
    std::unique_ptr<academia::CourseService> course_service;
    int64_t faculty_id, admin_id, student_id;
    academia::User faculty_user, admin_user, student_user;
};

TEST_F(CourseTest, CreateCourse) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "Intro to CS";
    course.credits = 3;
    course.faculty_id = faculty_id;
    course.capacity = 30;

    auto result = course_service->create_course(course, faculty_user);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.course_id, 0);
}

TEST_F(CourseTest, CreateDuplicateCourse) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "Intro to CS";
    course.credits = 3;
    course.faculty_id = faculty_id;
    course.capacity = 30;

    course_service->create_course(course, faculty_user);
    auto result = course_service->create_course(course, faculty_user);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.find("already exists") != std::string::npos);
}

TEST_F(CourseTest, StudentCannotCreateCourse) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "Intro to CS";
    course.credits = 3;
    course.capacity = 30;

    auto result = course_service->create_course(course, student_user);
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.find("Unauthorized") != std::string::npos);
}

TEST_F(CourseTest, InvalidCapacity) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "Intro to CS";
    course.credits = 3;
    course.capacity = 0;

    auto result = course_service->create_course(course, faculty_user);
    EXPECT_FALSE(result.success);
}

TEST_F(CourseTest, GetAllCourses) {
    academia::Course c1;
    c1.course_code = "CS101";
    c1.course_name = "Intro";
    c1.credits = 3;
    c1.faculty_id = faculty_id;
    c1.capacity = 30;
    course_service->create_course(c1, faculty_user);

    academia::Course c2;
    c2.course_code = "CS201";
    c2.course_name = "Advanced";
    c2.credits = 4;
    c2.faculty_id = faculty_id;
    c2.capacity = 25;
    course_service->create_course(c2, faculty_user);

    auto courses = course_service->get_all_courses();
    EXPECT_EQ(courses.size(), 2);
}

TEST_F(CourseTest, SearchCourses) {
    academia::Course c1;
    c1.course_code = "CS101";
    c1.course_name = "Intro to Programming";
    c1.credits = 3;
    c1.faculty_id = faculty_id;
    c1.capacity = 30;
    course_service->create_course(c1, faculty_user);

    auto results = course_service->search_courses("Programming");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].course_name, "Intro to Programming");
}

TEST_F(CourseTest, UpdateCourse) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "Old Name";
    course.credits = 3;
    course.faculty_id = faculty_id;
    course.capacity = 30;
    auto create_result = course_service->create_course(course, faculty_user);

    academia::Course update;
    update.id = create_result.course_id;
    update.course_name = "New Name";
    update.credits = 4;
    update.capacity = 40;

    auto result = course_service->update_course(update, faculty_user);
    EXPECT_TRUE(result.success);
}

TEST_F(CourseTest, DeleteCourse) {
    academia::Course course;
    course.course_code = "CS101";
    course.course_name = "To Delete";
    course.credits = 3;
    course.faculty_id = faculty_id;
    course.capacity = 30;
    auto create_result = course_service->create_course(course, faculty_user);

    auto result = course_service->delete_course(create_result.course_id, admin_user);
    EXPECT_TRUE(result.success);

    auto found = course_service->get_course(create_result.course_id);
    EXPECT_FALSE(found.has_value());
}
