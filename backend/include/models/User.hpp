#pragma once
/**
 * User.hpp — User domain model
 */

#include <string>
#include <nlohmann/json.hpp>

namespace academia {

enum class Role {
    Admin,
    Faculty,
    Student
};

inline std::string role_to_string(Role r) {
    switch (r) {
        case Role::Admin:   return "admin";
        case Role::Faculty: return "faculty";
        case Role::Student: return "student";
    }
    return "unknown";
}

inline Role string_to_role(const std::string& s) {
    if (s == "admin")   return Role::Admin;
    if (s == "faculty") return Role::Faculty;
    if (s == "student") return Role::Student;
    throw std::invalid_argument("Invalid role: " + s);
}

struct User {
    int64_t id = 0;
    std::string username;
    std::string password_hash;
    Role role = Role::Student;
    std::string name;
    std::string email;
    bool active = true;
    std::string created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"username", username},
            {"role", role_to_string(role)},
            {"name", name},
            {"email", email},
            {"active", active},
            {"created_at", created_at}
        };
        // NOTE: password_hash is NEVER serialized to JSON
    }

    static User from_json(const nlohmann::json& j) {
        User u;
        if (j.contains("id")) u.id = j["id"].get<int64_t>();
        if (j.contains("username")) u.username = j["username"].get<std::string>();
        if (j.contains("role")) u.role = string_to_role(j["role"].get<std::string>());
        if (j.contains("name")) u.name = j["name"].get<std::string>();
        if (j.contains("email")) u.email = j["email"].get<std::string>();
        if (j.contains("active")) u.active = j["active"].get<bool>();
        if (j.contains("password")) u.password_hash = j["password"].get<std::string>();
        return u;
    }
};

} // namespace academia
