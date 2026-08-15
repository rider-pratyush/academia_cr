#pragma once
/**
 * RegistrationController.hpp — Registration REST endpoints
 */

#include "services/RegistrationService.hpp"
#include "services/AuthService.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "middleware/AuthMiddleware.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>

namespace academia {

class RegistrationController {
public:
    RegistrationController(RegistrationService& reg_service, AuthService& auth_service,
                           RegistrationRepository& reg_repo)
        : reg_service_(reg_service), auth_service_(auth_service), reg_repo_(reg_repo) {}

    void register_routes(crow::SimpleApp& app) {
        // POST /api/courses/<int>/register — register for a course
        CROW_ROUTE(app, "/api/courses/<int>/register").methods("POST"_method)
        ([this](const crow::request& req, int course_id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role != Role::Student) {
                return crow::response(403, error_response("Only students can register for courses", 403).dump());
            }

            auto result = reg_service_.register_student(user->id, course_id);
            if (!result.success) {
                int status = 400;
                if (result.error.find("full") != std::string::npos) status = 409;
                if (result.error.find("Already") != std::string::npos) status = 409;
                if (result.error.find("not found") != std::string::npos) status = 404;
                return crow::response(status, error_response(result.error, status).dump());
            }

            nlohmann::json data = {
                {"registration_id", result.registration_id},
                {"remaining_seats", result.remaining_seats}
            };
            auto resp = crow::response(201, success_response(data, "Registration successful").dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // DELETE /api/courses/<int>/register — drop a course
        CROW_ROUTE(app, "/api/courses/<int>/register").methods("DELETE"_method)
        ([this](const crow::request& req, int course_id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role != Role::Student) {
                return crow::response(403, error_response("Only students can drop courses", 403).dump());
            }

            auto result = reg_service_.drop_student(user->id, course_id);
            if (!result.success) {
                return crow::response(400, error_response(result.error).dump());
            }

            nlohmann::json data = {{"remaining_seats", result.remaining_seats}};
            auto resp = crow::response(200, success_response(data, "Course dropped").dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/students/<int>/courses — get student's enrolled courses
        CROW_ROUTE(app, "/api/students/<int>/courses").methods("GET"_method)
        ([this](const crow::request& req, int student_id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            // Students can only view their own, admin/faculty can view any
            if (user->role == Role::Student && user->id != student_id) {
                return crow::response(403, error_response("Forbidden", 403).dump());
            }

            auto regs = reg_service_.get_student_registrations(student_id);
            nlohmann::json regs_json = nlohmann::json::array();
            for (const auto& r : regs) {
                regs_json.push_back(r.to_json());
            }

            int total_credits = reg_repo_.total_credits_for_student(student_id);
            nlohmann::json data = {
                {"registrations", regs_json},
                {"total_credits", total_credits},
                {"course_count", regs.size()}
            };

            auto resp = crow::response(200, success_response(data).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/faculty/<int>/courses — get faculty's courses with enrollment info
        CROW_ROUTE(app, "/api/faculty/<int>/courses").methods("GET"_method)
        ([this](const crow::request& req, int faculty_id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role == Role::Student) {
                return crow::response(403, error_response("Forbidden", 403).dump());
            }

            auto courses = reg_service_.get_course_enrollments(0); // placeholder
            // Actually get faculty courses from course service
            nlohmann::json data = nlohmann::json::array();
            auto resp = crow::response(200, success_response(data).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/courses/<int>/students — get enrolled students
        CROW_ROUTE(app, "/api/courses/<int>/students").methods("GET"_method)
        ([this](const crow::request& req, int course_id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role == Role::Student) {
                return crow::response(403, error_response("Forbidden", 403).dump());
            }

            auto regs = reg_service_.get_course_enrollments(course_id);
            nlohmann::json regs_json = nlohmann::json::array();
            for (const auto& r : regs) {
                regs_json.push_back(r.to_json());
            }

            auto resp = crow::response(200, success_response(regs_json).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });
    }

private:
    RegistrationService& reg_service_;
    AuthService& auth_service_;
    RegistrationRepository& reg_repo_;
};

} // namespace academia
