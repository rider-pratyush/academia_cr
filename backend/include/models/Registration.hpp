#pragma once
/**
 * Registration.hpp — Registration domain model
 */

#include <string>
#include <nlohmann/json.hpp>

namespace academia {

struct Registration {
    int64_t id = 0;
    int64_t student_id = 0;
    int64_t course_id = 0;
    std::string status = "active";  // "active" or "dropped"
    std::string registered_at;

    // Joined fields for display
    std::string student_name;
    std::string course_code;
    std::string course_name;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"student_id", student_id},
            {"course_id", course_id},
            {"status", status},
            {"registered_at", registered_at},
            {"student_name", student_name},
            {"course_code", course_code},
            {"course_name", course_name}
        };
    }
};

} // namespace academia
