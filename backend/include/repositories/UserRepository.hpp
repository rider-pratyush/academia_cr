#pragma once
/**
 * UserRepository.hpp — Data access layer for users
 * 
 * REPOSITORY PATTERN: Encapsulates all SQL queries for the users table.
 * Controllers never write raw SQL — they call repository methods.
 * This separates data access concerns from business logic.
 */

#include "database/Database.hpp"
#include "models/User.hpp"
#include <vector>
#include <optional>

namespace academia {

class UserRepository {
public:
    explicit UserRepository(Database& db) : db_(db) {}

    std::optional<User> find_by_id(int64_t id) {
        auto stmt = db_.prepare(
            "SELECT id, username, password_hash, role, name, email, active, created_at "
            "FROM users WHERE id = ?");
        stmt.bind(1, id);
        if (stmt.step() == SQLITE_ROW) {
            return row_to_user(stmt);
        }
        return std::nullopt;
    }

    std::optional<User> find_by_username(const std::string& username) {
        auto stmt = db_.prepare(
            "SELECT id, username, password_hash, role, name, email, active, created_at "
            "FROM users WHERE username = ?");
        stmt.bind(1, username);
        if (stmt.step() == SQLITE_ROW) {
            return row_to_user(stmt);
        }
        return std::nullopt;
    }

    std::vector<User> find_all() {
        auto stmt = db_.prepare(
            "SELECT id, username, password_hash, role, name, email, active, created_at "
            "FROM users ORDER BY id");
        std::vector<User> users;
        while (stmt.step() == SQLITE_ROW) {
            users.push_back(row_to_user(stmt));
        }
        return users;
    }

    std::vector<User> find_by_role(Role role) {
        auto stmt = db_.prepare(
            "SELECT id, username, password_hash, role, name, email, active, created_at "
            "FROM users WHERE role = ? ORDER BY name");
        stmt.bind(1, role_to_string(role));
        std::vector<User> users;
        while (stmt.step() == SQLITE_ROW) {
            users.push_back(row_to_user(stmt));
        }
        return users;
    }

    int64_t create(const User& user) {
        auto stmt = db_.prepare(
            "INSERT INTO users (username, password_hash, role, name, email, active) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        stmt.bind(1, user.username);
        stmt.bind(2, user.password_hash);
        stmt.bind(3, role_to_string(user.role));
        stmt.bind(4, user.name);
        stmt.bind(5, user.email);
        stmt.bind(6, user.active ? 1 : 0);
        
        if (stmt.step() != SQLITE_DONE) {
            throw std::runtime_error("Failed to create user");
        }
        return db_.last_insert_id();
    }

    bool update(const User& user) {
        auto stmt = db_.prepare(
            "UPDATE users SET name = ?, email = ?, active = ? WHERE id = ?");
        stmt.bind(1, user.name);
        stmt.bind(2, user.email);
        stmt.bind(3, user.active ? 1 : 0);
        stmt.bind(4, user.id);
        return stmt.step() == SQLITE_DONE;
    }

    bool update_password(int64_t id, const std::string& password_hash) {
        auto stmt = db_.prepare(
            "UPDATE users SET password_hash = ? WHERE id = ?");
        stmt.bind(1, password_hash);
        stmt.bind(2, id);
        return stmt.step() == SQLITE_DONE;
    }

    bool remove(int64_t id) {
        auto stmt = db_.prepare("DELETE FROM users WHERE id = ?");
        stmt.bind(1, id);
        return stmt.step() == SQLITE_DONE;
    }

    int count() {
        auto stmt = db_.prepare("SELECT COUNT(*) FROM users");
        stmt.step();
        return stmt.column_int(0);
    }

    int count_by_role(Role role) {
        auto stmt = db_.prepare("SELECT COUNT(*) FROM users WHERE role = ?");
        stmt.bind(1, role_to_string(role));
        stmt.step();
        return stmt.column_int(0);
    }

    // Session management
    void create_session(const std::string& token, int64_t user_id) {
        auto stmt = db_.prepare(
            "INSERT OR REPLACE INTO sessions (token, user_id, expires_at) "
            "VALUES (?, ?, datetime('now', '+24 hours'))");
        stmt.bind(1, token);
        stmt.bind(2, user_id);
        stmt.step();
    }

    std::optional<int64_t> validate_session(const std::string& token) {
        auto stmt = db_.prepare(
            "SELECT user_id FROM sessions "
            "WHERE token = ? AND (expires_at IS NULL OR expires_at > datetime('now'))");
        stmt.bind(1, token);
        if (stmt.step() == SQLITE_ROW) {
            return stmt.column_int64(0);
        }
        return std::nullopt;
    }

    void delete_session(const std::string& token) {
        auto stmt = db_.prepare("DELETE FROM sessions WHERE token = ?");
        stmt.bind(1, token);
        stmt.step();
    }

    void cleanup_expired_sessions() {
        db_.execute("DELETE FROM sessions WHERE expires_at < datetime('now')");
    }

private:
    User row_to_user(PreparedStatement& stmt) {
        User u;
        u.id = stmt.column_int64(0);
        u.username = stmt.column_text(1);
        u.password_hash = stmt.column_text(2);
        u.role = string_to_role(stmt.column_text(3));
        u.name = stmt.column_text(4);
        u.email = stmt.column_text(5);
        u.active = stmt.column_int(6) != 0;
        u.created_at = stmt.column_text(7);
        return u;
    }

    Database& db_;
};

} // namespace academia
