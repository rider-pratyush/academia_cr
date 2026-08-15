#pragma once
/**
 * CourseRepository.hpp — Data access layer for courses
 */

#include "database/Database.hpp"
#include "models/Course.hpp"
#include <vector>
#include <optional>

namespace academia {

class CourseRepository {
public:
    explicit CourseRepository(Database& db) : db_(db) {}

    std::optional<Course> find_by_id(int64_t id) {
        auto stmt = db_.prepare(
            "SELECT c.id, c.course_code, c.course_name, c.credits, c.faculty_id, "
            "COALESCE(u.name, ''), c.capacity, "
            "(SELECT COUNT(*) FROM registrations r WHERE r.course_id = c.id AND r.status = 'active'), "
            "c.schedule, c.created_at "
            "FROM courses c LEFT JOIN users u ON c.faculty_id = u.id "
            "WHERE c.id = ?");
        stmt.bind(1, id);
        if (stmt.step() == SQLITE_ROW) {
            return row_to_course(stmt);
        }
        return std::nullopt;
    }

    std::optional<Course> find_by_code(const std::string& code) {
        auto stmt = db_.prepare(
            "SELECT c.id, c.course_code, c.course_name, c.credits, c.faculty_id, "
            "COALESCE(u.name, ''), c.capacity, "
            "(SELECT COUNT(*) FROM registrations r WHERE r.course_id = c.id AND r.status = 'active'), "
            "c.schedule, c.created_at "
            "FROM courses c LEFT JOIN users u ON c.faculty_id = u.id "
            "WHERE c.course_code = ?");
        stmt.bind(1, code);
        if (stmt.step() == SQLITE_ROW) {
            return row_to_course(stmt);
        }
        return std::nullopt;
    }

    std::vector<Course> find_all() {
        auto stmt = db_.prepare(
            "SELECT c.id, c.course_code, c.course_name, c.credits, c.faculty_id, "
            "COALESCE(u.name, ''), c.capacity, "
            "(SELECT COUNT(*) FROM registrations r WHERE r.course_id = c.id AND r.status = 'active'), "
            "c.schedule, c.created_at "
            "FROM courses c LEFT JOIN users u ON c.faculty_id = u.id "
            "ORDER BY c.course_code");
        std::vector<Course> courses;
        while (stmt.step() == SQLITE_ROW) {
            courses.push_back(row_to_course(stmt));
        }
        return courses;
    }

    std::vector<Course> find_by_faculty(int64_t faculty_id) {
        auto stmt = db_.prepare(
            "SELECT c.id, c.course_code, c.course_name, c.credits, c.faculty_id, "
            "COALESCE(u.name, ''), c.capacity, "
            "(SELECT COUNT(*) FROM registrations r WHERE r.course_id = c.id AND r.status = 'active'), "
            "c.schedule, c.created_at "
            "FROM courses c LEFT JOIN users u ON c.faculty_id = u.id "
            "WHERE c.faculty_id = ? ORDER BY c.course_code");
        stmt.bind(1, faculty_id);
        std::vector<Course> courses;
        while (stmt.step() == SQLITE_ROW) {
            courses.push_back(row_to_course(stmt));
        }
        return courses;
    }

    std::vector<Course> search(const std::string& query) {
        auto stmt = db_.prepare(
            "SELECT c.id, c.course_code, c.course_name, c.credits, c.faculty_id, "
            "COALESCE(u.name, ''), c.capacity, "
            "(SELECT COUNT(*) FROM registrations r WHERE r.course_id = c.id AND r.status = 'active'), "
            "c.schedule, c.created_at "
            "FROM courses c LEFT JOIN users u ON c.faculty_id = u.id "
            "WHERE c.course_code LIKE ? OR c.course_name LIKE ? OR u.name LIKE ? "
            "ORDER BY c.course_code");
        std::string pattern = "%" + query + "%";
        stmt.bind(1, pattern);
        stmt.bind(2, pattern);
        stmt.bind(3, pattern);
        std::vector<Course> courses;
        while (stmt.step() == SQLITE_ROW) {
            courses.push_back(row_to_course(stmt));
        }
        return courses;
    }

    int64_t create(const Course& course) {
        auto stmt = db_.prepare(
            "INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        stmt.bind(1, course.course_code);
        stmt.bind(2, course.course_name);
        stmt.bind(3, course.credits);
        stmt.bind(4, course.faculty_id);
        stmt.bind(5, course.capacity);
        stmt.bind(6, course.schedule);
        if (stmt.step() != SQLITE_DONE) {
            throw std::runtime_error("Failed to create course");
        }
        return db_.last_insert_id();
    }

    bool update(const Course& course) {
        auto stmt = db_.prepare(
            "UPDATE courses SET course_name = ?, credits = ?, faculty_id = ?, "
            "capacity = ?, schedule = ? WHERE id = ?");
        stmt.bind(1, course.course_name);
        stmt.bind(2, course.credits);
        stmt.bind(3, course.faculty_id);
        stmt.bind(4, course.capacity);
        stmt.bind(5, course.schedule);
        stmt.bind(6, course.id);
        return stmt.step() == SQLITE_DONE;
    }

    bool remove(int64_t id) {
        auto stmt = db_.prepare("DELETE FROM courses WHERE id = ?");
        stmt.bind(1, id);
        return stmt.step() == SQLITE_DONE;
    }

    int count() {
        auto stmt = db_.prepare("SELECT COUNT(*) FROM courses");
        stmt.step();
        return stmt.column_int(0);
    }

    /**
     * Get enrolled count for a course. This is used within transactions
     * for race-condition-safe registration.
     */
    int get_enrolled_count(int64_t course_id) {
        auto stmt = db_.prepare(
            "SELECT COUNT(*) FROM registrations "
            "WHERE course_id = ? AND status = 'active'");
        stmt.bind(1, course_id);
        stmt.step();
        return stmt.column_int(0);
    }

private:
    Course row_to_course(PreparedStatement& stmt) {
        Course c;
        c.id = stmt.column_int64(0);
        c.course_code = stmt.column_text(1);
        c.course_name = stmt.column_text(2);
        c.credits = stmt.column_int(3);
        c.faculty_id = stmt.column_int64(4);
        c.faculty_name = stmt.column_text(5);
        c.capacity = stmt.column_int(6);
        c.enrolled_count = stmt.column_int(7);
        c.schedule = stmt.column_text(8);
        c.created_at = stmt.column_text(9);
        return c;
    }

    Database& db_;
};

} // namespace academia
