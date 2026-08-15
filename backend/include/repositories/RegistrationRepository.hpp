#pragma once
/**
 * RegistrationRepository.hpp — Data access layer for registrations
 */

#include "database/Database.hpp"
#include "models/Registration.hpp"
#include <vector>
#include <optional>

namespace academia {

class RegistrationRepository {
public:
    explicit RegistrationRepository(Database& db) : db_(db) {}

    std::optional<Registration> find_by_id(int64_t id) {
        auto stmt = db_.prepare(
            "SELECT r.id, r.student_id, r.course_id, r.status, r.registered_at, "
            "u.name, c.course_code, c.course_name "
            "FROM registrations r "
            "JOIN users u ON r.student_id = u.id "
            "JOIN courses c ON r.course_id = c.id "
            "WHERE r.id = ?");
        stmt.bind(1, id);
        if (stmt.step() == SQLITE_ROW) {
            return row_to_registration(stmt);
        }
        return std::nullopt;
    }

    std::vector<Registration> find_by_student(int64_t student_id, const std::string& status = "active") {
        auto stmt = db_.prepare(
            "SELECT r.id, r.student_id, r.course_id, r.status, r.registered_at, "
            "u.name, c.course_code, c.course_name "
            "FROM registrations r "
            "JOIN users u ON r.student_id = u.id "
            "JOIN courses c ON r.course_id = c.id "
            "WHERE r.student_id = ? AND r.status = ? "
            "ORDER BY r.registered_at DESC");
        stmt.bind(1, student_id);
        stmt.bind(2, status);
        std::vector<Registration> regs;
        while (stmt.step() == SQLITE_ROW) {
            regs.push_back(row_to_registration(stmt));
        }
        return regs;
    }

    std::vector<Registration> find_by_course(int64_t course_id, const std::string& status = "active") {
        auto stmt = db_.prepare(
            "SELECT r.id, r.student_id, r.course_id, r.status, r.registered_at, "
            "u.name, c.course_code, c.course_name "
            "FROM registrations r "
            "JOIN users u ON r.student_id = u.id "
            "JOIN courses c ON r.course_id = c.id "
            "WHERE r.course_id = ? AND r.status = ? "
            "ORDER BY u.name");
        stmt.bind(1, course_id);
        stmt.bind(2, status);
        std::vector<Registration> regs;
        while (stmt.step() == SQLITE_ROW) {
            regs.push_back(row_to_registration(stmt));
        }
        return regs;
    }

    std::vector<Registration> find_all(const std::string& status = "active") {
        auto stmt = db_.prepare(
            "SELECT r.id, r.student_id, r.course_id, r.status, r.registered_at, "
            "u.name, c.course_code, c.course_name "
            "FROM registrations r "
            "JOIN users u ON r.student_id = u.id "
            "JOIN courses c ON r.course_id = c.id "
            "WHERE r.status = ? "
            "ORDER BY r.registered_at DESC");
        stmt.bind(1, status);
        std::vector<Registration> regs;
        while (stmt.step() == SQLITE_ROW) {
            regs.push_back(row_to_registration(stmt));
        }
        return regs;
    }

    bool exists(int64_t student_id, int64_t course_id) {
        auto stmt = db_.prepare(
            "SELECT COUNT(*) FROM registrations "
            "WHERE student_id = ? AND course_id = ? AND status = 'active'");
        stmt.bind(1, student_id);
        stmt.bind(2, course_id);
        stmt.step();
        return stmt.column_int(0) > 0;
    }

    int64_t create(int64_t student_id, int64_t course_id) {
        // First, try to reactivate a previously dropped registration.
        // The UNIQUE(student_id, course_id) constraint means the old
        // 'dropped' row still occupies the slot — a plain INSERT would
        // fail with a constraint violation.
        auto update_stmt = db_.prepare(
            "UPDATE registrations SET status = 'active', "
            "registered_at = datetime('now') "
            "WHERE student_id = ? AND course_id = ? AND status = 'dropped'");
        update_stmt.bind(1, student_id);
        update_stmt.bind(2, course_id);
        update_stmt.step();

        if (db_.changes() > 0) {
            // Reactivated existing dropped registration
            auto id_stmt = db_.prepare(
                "SELECT id FROM registrations WHERE student_id = ? AND course_id = ?");
            id_stmt.bind(1, student_id);
            id_stmt.bind(2, course_id);
            id_stmt.step();
            return id_stmt.column_int64(0);
        }

        // No dropped registration existed — insert a new one
        auto stmt = db_.prepare(
            "INSERT INTO registrations (student_id, course_id, status) "
            "VALUES (?, ?, 'active')");
        stmt.bind(1, student_id);
        stmt.bind(2, course_id);
        if (stmt.step() != SQLITE_DONE) {
            throw std::runtime_error("Failed to create registration");
        }
        return db_.last_insert_id();
    }

    bool drop_registration(int64_t student_id, int64_t course_id) {
        auto stmt = db_.prepare(
            "UPDATE registrations SET status = 'dropped' "
            "WHERE student_id = ? AND course_id = ? AND status = 'active'");
        stmt.bind(1, student_id);
        stmt.bind(2, course_id);
        return stmt.step() == SQLITE_DONE;
    }

    bool remove(int64_t id) {
        auto stmt = db_.prepare("DELETE FROM registrations WHERE id = ?");
        stmt.bind(1, id);
        return stmt.step() == SQLITE_DONE;
    }

    int count_active() {
        auto stmt = db_.prepare("SELECT COUNT(*) FROM registrations WHERE status = 'active'");
        stmt.step();
        return stmt.column_int(0);
    }

    int count_by_student(int64_t student_id) {
        auto stmt = db_.prepare(
            "SELECT COUNT(*) FROM registrations WHERE student_id = ? AND status = 'active'");
        stmt.bind(1, student_id);
        stmt.step();
        return stmt.column_int(0);
    }

    int total_credits_for_student(int64_t student_id) {
        auto stmt = db_.prepare(
            "SELECT COALESCE(SUM(c.credits), 0) FROM registrations r "
            "JOIN courses c ON r.course_id = c.id "
            "WHERE r.student_id = ? AND r.status = 'active'");
        stmt.bind(1, student_id);
        stmt.step();
        return stmt.column_int(0);
    }

private:
    Registration row_to_registration(PreparedStatement& stmt) {
        Registration r;
        r.id = stmt.column_int64(0);
        r.student_id = stmt.column_int64(1);
        r.course_id = stmt.column_int64(2);
        r.status = stmt.column_text(3);
        r.registered_at = stmt.column_text(4);
        r.student_name = stmt.column_text(5);
        r.course_code = stmt.column_text(6);
        r.course_name = stmt.column_text(7);
        return r;
    }

    Database& db_;
};

} // namespace academia
