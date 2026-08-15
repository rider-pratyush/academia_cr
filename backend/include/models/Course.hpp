#pragma once
/**
 * Course.hpp — Course domain model
 */

#include <string>
#include <nlohmann/json.hpp>

namespace academia {

struct Course {
    int64_t id = 0;
    std::string course_code;
    std::string course_name;
    int credits = 3;
    int64_t faculty_id = 0;
    std::string faculty_name;  // Joined from users table
    int capacity = 0;
    int enrolled_count = 0;    // Computed from registrations
    std::string schedule;
    std::string created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"course_code", course_code},
            {"course_name", course_name},
            {"credits", credits},
            {"faculty_id", faculty_id},
            {"faculty_name", faculty_name},
            {"capacity", capacity},
            {"enrolled_count", enrolled_count},
            {"available_seats", capacity - enrolled_count},
            {"schedule", schedule},
            {"created_at", created_at}
        };
    }

    static Course from_json(const nlohmann::json& j) {
        Course c;
        if (j.contains("id")) c.id = j["id"].get<int64_t>();
        if (j.contains("course_code")) c.course_code = j["course_code"].get<std::string>();
        if (j.contains("course_name")) c.course_name = j["course_name"].get<std::string>();
        if (j.contains("credits")) c.credits = j["credits"].get<int>();
        if (j.contains("faculty_id")) c.faculty_id = j["faculty_id"].get<int64_t>();
        if (j.contains("capacity")) c.capacity = j["capacity"].get<int>();
        if (j.contains("schedule")) c.schedule = j["schedule"].get<std::string>();
        return c;
    }
};

} // namespace academia
