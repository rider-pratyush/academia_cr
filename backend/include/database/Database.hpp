#pragma once
/**
 * Database.hpp — RAII SQLite database wrapper
 * 
 * OS CONCEPTS:
 *   - RAII: The Database object owns the sqlite3* connection. Constructor opens,
 *     destructor closes. No manual cleanup needed — prevents resource leaks.
 *   - TRANSACTIONS: Wraps multi-statement operations atomically. Either all
 *     statements succeed (commit) or all are rolled back — critical for
 *     preventing partial updates during concurrent registrations.
 *   - PREPARED STATEMENTS: Parameterized queries prevent SQL injection and
 *     improve performance through query plan caching.
 */

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <sqlite3.h>

namespace academia {

/**
 * RAII wrapper for sqlite3_stmt (prepared statement).
 * Automatically finalizes the statement on destruction.
 */
class PreparedStatement {
public:
    PreparedStatement() = default;
    
    explicit PreparedStatement(sqlite3_stmt* stmt) : stmt_(stmt) {}

    ~PreparedStatement() {
        if (stmt_) sqlite3_finalize(stmt_);
    }

    // Move-only
    PreparedStatement(PreparedStatement&& other) noexcept : stmt_(other.stmt_) {
        other.stmt_ = nullptr;
    }
    PreparedStatement& operator=(PreparedStatement&& other) noexcept {
        if (this != &other) {
            if (stmt_) sqlite3_finalize(stmt_);
            stmt_ = other.stmt_;
            other.stmt_ = nullptr;
        }
        return *this;
    }
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    void bind(int index, int value) {
        sqlite3_bind_int(stmt_, index, value);
    }
    void bind(int index, const std::string& value) {
        sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    void bind(int index, int64_t value) {
        sqlite3_bind_int64(stmt_, index, value);
    }
    void bind_null(int index) {
        sqlite3_bind_null(stmt_, index);
    }

    int step() { return sqlite3_step(stmt_); }
    void reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }

    int column_int(int col) { return sqlite3_column_int(stmt_, col); }
    int64_t column_int64(int col) { return sqlite3_column_int64(stmt_, col); }
    std::string column_text(int col) {
        auto text = sqlite3_column_text(stmt_, col);
        return text ? std::string(reinterpret_cast<const char*>(text)) : "";
    }
    int column_count() { return sqlite3_column_count(stmt_); }

    sqlite3_stmt* get() { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

/**
 * RAII SQLite database connection.
 */
class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    // Non-copyable, non-movable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /** Prepare a SQL statement for execution. */
    PreparedStatement prepare(const std::string& sql);

    /** Execute a SQL statement directly (no results expected). */
    void execute(const std::string& sql);

    /** Begin a transaction. Uses IMMEDIATE to acquire a write lock immediately,
     *  preventing other writers from starting transactions concurrently.
     *  This is critical for race-condition prevention in registration. */
    void begin_transaction();

    /** Commit the current transaction. */
    void commit();

    /** Rollback the current transaction. */
    void rollback();

    /** Execute a function within a transaction. Automatically commits on success,
     *  rolls back on exception. */
    template<typename Func>
    auto transaction(Func&& func) -> decltype(func()) {
        begin_transaction();
        try {
            if constexpr (std::is_void_v<decltype(func())>) {
                func();
                commit();
            } else {
                auto result = func();
                commit();
                return result;
            }
        } catch (...) {
            rollback();
            throw;
        }
    }

    /** Get the number of rows modified by the last INSERT/UPDATE/DELETE. */
    int changes() { return sqlite3_changes(db_); }

    /** Get the last inserted row ID. */
    int64_t last_insert_id();

    /** Initialize the database schema. */
    void initialize_schema();

    /** Seed the database with demo data. */
    void seed_data();

    /** Get the raw sqlite3 handle (for advanced use). */
    sqlite3* handle() { return db_; }

    /** Get the database mutex for external synchronization. */
    std::mutex& mutex() { return db_mutex_; }

private:
    sqlite3* db_ = nullptr;
    std::mutex db_mutex_;  // Serializes write operations
};

} // namespace academia
