#pragma once
/**
 * AuthController.hpp — Authentication REST endpoints
 */

#include "services/AuthService.hpp"
#include "middleware/AuthMiddleware.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>

namespace academia {

class AuthController {
public:
    explicit AuthController(AuthService& auth_service) : auth_service_(auth_service) {}

    void register_routes(crow::SimpleApp& app) {
        // POST /api/auth/login
        CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
        ([this](const crow::request& req) {
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string username = body.value("username", "");
                std::string password = body.value("password", "");

                if (username.empty() || password.empty()) {
                    return crow::response(400, error_response("Username and password are required").dump());
                }

                auto result = auth_service_.login(username, password);
                if (!result.success) {
                    return crow::response(401, error_response(result.error, 401).dump());
                }

                nlohmann::json data = {
                    {"token", result.token},
                    {"user", result.user.to_json()}
                };
                auto resp = crow::response(200, success_response(data, "Login successful").dump());
                resp.add_header("Content-Type", "application/json");
                return resp;
            } catch (const nlohmann::json::exception& e) {
                return crow::response(400, error_response("Invalid JSON body").dump());
            }
        });

        // POST /api/auth/logout
        CROW_ROUTE(app, "/api/auth/logout").methods("POST"_method)
        ([this](const crow::request& req) {
            std::string auth = req.get_header_value("Authorization");
            if (auth.size() > 7) {
                auth_service_.logout(auth.substr(7));
            }
            auto resp = crow::response(200, success_response(nullptr, "Logged out").dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/auth/me
        CROW_ROUTE(app, "/api/auth/me").methods("GET"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            auto resp = crow::response(200, success_response(user->to_json()).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });
    }

private:
    AuthService& auth_service_;
};

} // namespace academia
