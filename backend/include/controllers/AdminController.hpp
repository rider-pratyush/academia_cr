#pragma once
/**
 * AdminController.hpp — Admin-only REST endpoints
 */

#include "services/AuthService.hpp"
#include "services/CourseService.hpp"
#include "repositories/UserRepository.hpp"
#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "middleware/AuthMiddleware.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>

namespace academia {

class AdminController {
public:
    AdminController(AuthService& auth_service, UserRepository& user_repo,
                    CourseRepository& course_repo, RegistrationRepository& reg_repo)
        : auth_service_(auth_service), user_repo_(user_repo),
          course_repo_(course_repo), reg_repo_(reg_repo) {}

    void register_routes(crow::SimpleApp& app) {
        // GET /api/admin/users — list all users
        CROW_ROUTE(app, "/api/admin/users").methods("GET"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user || user->role != Role::Admin) {
                return crow::response(403, error_response("Admin access required", 403).dump());
            }

            auto users = user_repo_.find_all();
            nlohmann::json users_json = nlohmann::json::array();
            for (const auto& u : users) {
                users_json.push_back(u.to_json());
            }

            auto resp = crow::response(200, success_response(users_json).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // POST /api/admin/users — create user
        CROW_ROUTE(app, "/api/admin/users").methods("POST"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user || user->role != Role::Admin) {
                return crow::response(403, error_response("Admin access required", 403).dump());
            }

            try {
                auto body = nlohmann::json::parse(req.body);
                User new_user = User::from_json(body);

                if (new_user.username.empty()) {
                    return crow::response(400, error_response("Username is required").dump());
                }
                if (new_user.name.empty()) {
                    return crow::response(400, error_response("Name is required").dump());
                }

                std::string password = body.value("password", "");
                if (password.empty()) {
                    return crow::response(400, error_response("Password is required").dump());
                }

                // Check duplicate username
                if (user_repo_.find_by_username(new_user.username)) {
                    return crow::response(409, error_response("Username already exists", 409).dump());
                }

                new_user.password_hash = AuthService::hash_password(password);
                int64_t id = user_repo_.create(new_user);

                auto created = user_repo_.find_by_id(id);
                auto resp = crow::response(201, success_response(
                    created ? created->to_json() : nlohmann::json{}, "User created").dump());
                resp.add_header("Content-Type", "application/json");
                return resp;
            } catch (const nlohmann::json::exception&) {
                return crow::response(400, error_response("Invalid JSON body").dump());
            }
        });

        // PUT /api/admin/users/<int> — update user
        CROW_ROUTE(app, "/api/admin/users/<int>").methods("PUT"_method)
        ([this](const crow::request& req, int id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user || user->role != Role::Admin) {
                return crow::response(403, error_response("Admin access required", 403).dump());
            }

            try {
                auto body = nlohmann::json::parse(req.body);
                auto existing = user_repo_.find_by_id(id);
                if (!existing) {
                    return crow::response(404, error_response("User not found", 404).dump());
                }

                if (body.contains("name")) existing->name = body["name"].get<std::string>();
                if (body.contains("email")) existing->email = body["email"].get<std::string>();
                if (body.contains("active")) existing->active = body["active"].get<bool>();

                user_repo_.update(*existing);

                // Handle password change
                if (body.contains("password") && !body["password"].get<std::string>().empty()) {
                    user_repo_.update_password(id, 
                        AuthService::hash_password(body["password"].get<std::string>()));
                }

                auto updated = user_repo_.find_by_id(id);
                auto resp = crow::response(200, success_response(
                    updated ? updated->to_json() : nlohmann::json{}, "User updated").dump());
                resp.add_header("Content-Type", "application/json");
                return resp;
            } catch (const nlohmann::json::exception&) {
                return crow::response(400, error_response("Invalid JSON body").dump());
            }
        });

        // DELETE /api/admin/users/<int> — delete user
        CROW_ROUTE(app, "/api/admin/users/<int>").methods("DELETE"_method)
        ([this](const crow::request& req, int id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user || user->role != Role::Admin) {
                return crow::response(403, error_response("Admin access required", 403).dump());
            }
            if (user->id == id) {
                return crow::response(400, error_response("Cannot delete your own account").dump());
            }

            user_repo_.remove(id);
            auto resp = crow::response(200, success_response(nullptr, "User deleted").dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/admin/statistics — system statistics
        CROW_ROUTE(app, "/api/admin/statistics").methods("GET"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user || user->role != Role::Admin) {
                return crow::response(403, error_response("Admin access required", 403).dump());
            }

            nlohmann::json stats = {
                {"total_users", user_repo_.count()},
                {"total_students", user_repo_.count_by_role(Role::Student)},
                {"total_faculty", user_repo_.count_by_role(Role::Faculty)},
                {"total_admins", user_repo_.count_by_role(Role::Admin)},
                {"total_courses", course_repo_.count()},
                {"total_registrations", reg_repo_.count_active()},
            };

            // Course utilization data
            auto courses = course_repo_.find_all();
            nlohmann::json utilization = nlohmann::json::array();
            int total_capacity = 0;
            int total_enrolled = 0;
            for (const auto& c : courses) {
                total_capacity += c.capacity;
                total_enrolled += c.enrolled_count;
                utilization.push_back({
                    {"course_code", c.course_code},
                    {"course_name", c.course_name},
                    {"capacity", c.capacity},
                    {"enrolled", c.enrolled_count},
                    {"utilization", c.capacity > 0 ? 
                        (c.enrolled_count * 100.0 / c.capacity) : 0.0}
                });
            }
            stats["course_utilization"] = utilization;
            stats["overall_capacity"] = total_capacity;
            stats["overall_enrolled"] = total_enrolled;
            stats["overall_utilization"] = total_capacity > 0 
                ? (total_enrolled * 100.0 / total_capacity) : 0.0;

            auto resp = crow::response(200, success_response(stats).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });
    }

private:
    AuthService& auth_service_;
    UserRepository& user_repo_;
    CourseRepository& course_repo_;
    RegistrationRepository& reg_repo_;
};

} // namespace academia
