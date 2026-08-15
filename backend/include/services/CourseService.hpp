#pragma once
/**
 * CourseService.hpp — Course business logic
 */

#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "models/User.hpp"
#include "utils/Logger.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace academia {

class CourseService {
public:
    CourseService(CourseRepository& course_repo, RegistrationRepository& reg_repo)
        : course_repo_(course_repo), reg_repo_(reg_repo) {}

    std::vector<Course> get_all_courses() {
        return course_repo_.find_all();
    }

    std::optional<Course> get_course(int64_t id) {
        return course_repo_.find_by_id(id);
    }

    std::vector<Course> search_courses(const std::string& query) {
        return course_repo_.search(query);
    }

    std::vector<Course> get_faculty_courses(int64_t faculty_id) {
        return course_repo_.find_by_faculty(faculty_id);
    }

    struct CourseResult {
        bool success = false;
        int64_t course_id = 0;
        std::string error;
    };

    CourseResult create_course(const Course& course, const User& requester) {
        if (requester.role != Role::Admin && requester.role != Role::Faculty) {
            return {false, 0, "Unauthorized: only admin or faculty can create courses"};
        }

        // Validate
        if (course.course_code.empty()) return {false, 0, "Course code is required"};
        if (course.course_name.empty()) return {false, 0, "Course name is required"};
        if (course.capacity <= 0) return {false, 0, "Capacity must be positive"};
        if (course.credits <= 0 || course.credits > 6) return {false, 0, "Credits must be between 1 and 6"};

        // Check duplicate
        if (course_repo_.find_by_code(course.course_code)) {
            return {false, 0, "Course code already exists"};
        }

        try {
            auto id = course_repo_.create(course);
            LOG_INFO("Course created: " + course.course_code + " by " + requester.username);
            return {true, id, ""};
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("Failed to create course: ") + e.what());
            return {false, 0, "Failed to create course"};
        }
    }

    CourseResult update_course(const Course& course, const User& requester) {
        auto existing = course_repo_.find_by_id(course.id);
        if (!existing) return {false, 0, "Course not found"};

        // Faculty can only update their own courses
        if (requester.role == Role::Faculty && existing->faculty_id != requester.id) {
            return {false, 0, "You can only update your own courses"};
        }

        if (course.course_name.empty()) return {false, 0, "Course name is required"};
        if (course.capacity <= 0) return {false, 0, "Capacity must be positive"};

        // Don't allow reducing capacity below current enrollment
        int enrolled = course_repo_.get_enrolled_count(course.id);
        if (course.capacity < enrolled) {
            return {false, 0, "Cannot reduce capacity below current enrollment (" + 
                    std::to_string(enrolled) + ")"};
        }

        course_repo_.update(course);
        LOG_INFO("Course updated: " + course.course_code + " by " + requester.username);
        return {true, course.id, ""};
    }

    CourseResult delete_course(int64_t id, const User& requester) {
        if (requester.role != Role::Admin) {
            auto existing = course_repo_.find_by_id(id);
            if (!existing) return {false, 0, "Course not found"};
            if (requester.role == Role::Faculty && existing->faculty_id != requester.id) {
                return {false, 0, "You can only delete your own courses"};
            }
        }

        course_repo_.remove(id);
        LOG_INFO("Course deleted: id=" + std::to_string(id) + " by " + requester.username);
        return {true, id, ""};
    }

    nlohmann::json get_course_statistics(int64_t course_id) {
        auto course = course_repo_.find_by_id(course_id);
        if (!course) return {{"error", "Course not found"}};

        auto students = reg_repo_.find_by_course(course_id);
        double utilization = course->capacity > 0 
            ? (static_cast<double>(course->enrolled_count) / course->capacity * 100.0) 
            : 0.0;

        return {
            {"course", course->to_json()},
            {"enrolled_students", static_cast<int>(students.size())},
            {"utilization_percent", utilization},
            {"available_seats", course->capacity - course->enrolled_count}
        };
    }

private:
    CourseRepository& course_repo_;
    RegistrationRepository& reg_repo_;
};

} // namespace academia
