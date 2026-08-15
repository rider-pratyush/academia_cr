#pragma once
/**
 * AuthMiddleware.hpp — HTTP authentication middleware
 * 
 * Extracts Bearer token from Authorization header, validates it,
 * and stores the authenticated user in the request context.
 */

#include "services/AuthService.hpp"
#include <string>
#include <optional>

namespace academia {

/**
 * Helper to extract and validate auth token from a request.
 * Returns the authenticated user or nullopt if invalid/missing.
 */
inline std::optional<User> authenticate_request(
    const std::string& auth_header, AuthService& auth_service) 
{
    if (auth_header.empty()) return std::nullopt;

    // Expected format: "Bearer <token>"
    const std::string prefix = "Bearer ";
    if (auth_header.substr(0, prefix.size()) != prefix) return std::nullopt;

    std::string token = auth_header.substr(prefix.size());
    if (token.empty()) return std::nullopt;

    return auth_service.validate_token(token);
}

/**
 * Create a JSON error response.
 */
inline nlohmann::json error_response(const std::string& message, int status = 400) {
    return {
        {"success", false},
        {"error", message},
        {"status", status}
    };
}

/**
 * Create a JSON success response.
 */
inline nlohmann::json success_response(const nlohmann::json& data = nullptr, 
                                        const std::string& message = "Success") {
    nlohmann::json response = {
        {"success", true},
        {"message", message}
    };
    if (!data.is_null()) {
        response["data"] = data;
    }
    return response;
}

} // namespace academia
