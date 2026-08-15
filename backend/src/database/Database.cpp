/**
 * Database.cpp — SQLite database implementation
 */

#include "database/Database.hpp"
#include "services/AuthService.hpp"
#include "utils/Logger.hpp"
#include <stdexcept>
#include <sstream>

namespace academia {

Database::Database(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err);
    }

    // Enable WAL mode for better concurrent read performance
    execute("PRAGMA journal_mode=WAL");
    // Enable foreign key enforcement
    execute("PRAGMA foreign_keys=ON");
    // Set busy timeout to 5 seconds (wait instead of failing on lock)
    sqlite3_busy_timeout(db_, 5000);

    LOG_INFO("Database opened: " + db_path);
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        LOG_INFO("Database connection closed");
    }
}

PreparedStatement Database::prepare(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + 
                                 std::string(sqlite3_errmsg(db_)) + 
                                 " SQL: " + sql);
    }
    return PreparedStatement(stmt);
}

void Database::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("SQL execution failed: " + err + " SQL: " + sql);
    }
}

void Database::begin_transaction() {
    // BEGIN IMMEDIATE acquires a RESERVED lock right away, which prevents
    // other connections from writing. This is critical for registration:
    // without IMMEDIATE, two transactions could both read capacity as
    // available, then both try to write, causing one to fail with SQLITE_BUSY.
    execute("BEGIN IMMEDIATE");
}

void Database::commit() {
    execute("COMMIT");
}

void Database::rollback() {
    try {
        execute("ROLLBACK");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Rollback failed: ") + e.what());
    }
}

int64_t Database::last_insert_id() {
    return sqlite3_last_insert_rowid(db_);
}

void Database::initialize_schema() {
    const char* schema = R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            role TEXT NOT NULL CHECK(role IN ('admin','faculty','student')),
            name TEXT NOT NULL,
            email TEXT DEFAULT '',
            active INTEGER DEFAULT 1,
            created_at TEXT DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS courses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            course_code TEXT UNIQUE NOT NULL,
            course_name TEXT NOT NULL,
            credits INTEGER DEFAULT 3,
            faculty_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
            capacity INTEGER NOT NULL CHECK(capacity > 0),
            schedule TEXT DEFAULT '',
            created_at TEXT DEFAULT (datetime('now'))
        );

        CREATE TABLE IF NOT EXISTS registrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            course_id INTEGER NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
            status TEXT DEFAULT 'active' CHECK(status IN ('active','dropped')),
            registered_at TEXT DEFAULT (datetime('now')),
            UNIQUE(student_id, course_id)
        );

        CREATE INDEX IF NOT EXISTS idx_registrations_course 
            ON registrations(course_id, status);
        CREATE INDEX IF NOT EXISTS idx_registrations_student 
            ON registrations(student_id, status);
        CREATE INDEX IF NOT EXISTS idx_courses_faculty 
            ON courses(faculty_id);
        CREATE INDEX IF NOT EXISTS idx_users_role 
            ON users(role);
        CREATE INDEX IF NOT EXISTS idx_users_username 
            ON users(username);

        -- Session tokens table
        CREATE TABLE IF NOT EXISTS sessions (
            token TEXT PRIMARY KEY,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            created_at TEXT DEFAULT (datetime('now')),
            expires_at TEXT
        );
    )SQL";

    execute(schema);
    LOG_INFO("Database schema initialized");
}

void Database::seed_data() {
    // Check if data already exists
    auto stmt = prepare("SELECT COUNT(*) FROM users");
    if (stmt.step() == SQLITE_ROW && stmt.column_int(0) > 0) {
        LOG_INFO("Database already seeded, skipping");
        return;
    }

    // Compute password hashes at runtime so they always match AuthService
    auto hp = [](const std::string& pw) { return AuthService::hash_password(pw); };

    std::string admin_hash   = hp("admin123");
    std::string faculty_hash = hp("faculty123");
    std::string student_hash = hp("student123");

    auto insert_user = prepare(
        "INSERT INTO users (username, password_hash, role, name, email) VALUES (?,?,?,?,?)");

    auto do_insert = [&](const std::string& uname, const std::string& hash,
                         const std::string& role, const std::string& name,
                         const std::string& email) {
        insert_user.reset();
        insert_user.bind(1, uname);
        insert_user.bind(2, hash);
        insert_user.bind(3, role);
        insert_user.bind(4, name);
        insert_user.bind(5, email);
        insert_user.step();
    };

    // Admin
    do_insert("admin", admin_hash, "admin", "System Administrator", "admin@academia.edu");

    // Faculty
    do_insert("dr.smith",  faculty_hash, "faculty", "Dr. Alice Smith",  "alice.smith@academia.edu");
    do_insert("dr.jones",  faculty_hash, "faculty", "Dr. Bob Jones",    "bob.jones@academia.edu");
    do_insert("dr.wilson", faculty_hash, "faculty", "Dr. Carol Wilson", "carol.wilson@academia.edu");

    // Students
    do_insert("john.doe",  student_hash, "student", "John Doe",  "john.doe@academia.edu");
    do_insert("jane.doe",  student_hash, "student", "Jane Doe",  "jane.doe@academia.edu");
    do_insert("mike.ross", student_hash, "student", "Mike Ross", "mike.ross@academia.edu");
    do_insert("sarah.lee", student_hash, "student", "Sarah Lee", "sarah.lee@academia.edu");
    do_insert("alex.kim",  student_hash, "student", "Alex Kim",  "alex.kim@academia.edu");

    // Courses (faculty_id references: admin=1, dr.smith=2, dr.jones=3, dr.wilson=4)
    execute(R"SQL(
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS301', 'Operating Systems', 4, 2, 40, 'Mon/Wed 10:00-11:30');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS302', 'Computer Networks', 3, 2, 35, 'Tue/Thu 14:00-15:30');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS201', 'Data Structures', 4, 3, 50, 'Mon/Wed/Fri 09:00-10:00');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS202', 'Algorithms', 3, 3, 45, 'Tue/Thu 10:00-11:30');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS401', 'Machine Learning', 3, 4, 30, 'Wed/Fri 14:00-15:30');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS402', 'Database Systems', 3, 4, 40, 'Mon/Wed 14:00-15:30');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS101', 'Intro to Programming', 3, 2, 60, 'Mon/Wed/Fri 11:00-12:00');
        INSERT INTO courses (course_code, course_name, credits, faculty_id, capacity, schedule) 
        VALUES ('CS501', 'Distributed Systems', 4, 3, 25, 'Tue/Thu 16:00-17:30');
    )SQL");

    // Sample registrations (student IDs: john.doe=5, jane.doe=6, mike.ross=7, sarah.lee=8, alex.kim=9)
    execute(R"SQL(
        INSERT INTO registrations (student_id, course_id, status) VALUES (5, 1, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (5, 3, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (5, 5, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (6, 1, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (6, 2, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (6, 4, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (7, 2, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (7, 3, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (8, 1, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (8, 5, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (8, 6, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (9, 4, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (9, 6, 'active');
        INSERT INTO registrations (student_id, course_id, status) VALUES (9, 7, 'active');
    )SQL");

    LOG_INFO("Database seeded with demo data (9 users, 8 courses, 14 registrations)");
}

} // namespace academia
