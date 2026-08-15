#pragma once
/**
 * CourseController.hpp — Course REST endpoints
 */

#include "services/CourseService.hpp"
#include "services/AuthService.hpp"
#include "middleware/AuthMiddleware.hpp"
#include <crow.h>
#include <nlohmann/json.hpp>

namespace academia {

class CourseController {
public:
    CourseController(CourseService& course_service, AuthService& auth_service)
        : course_service_(course_service), auth_service_(auth_service) {}

    void register_routes(crow::SimpleApp& app) {
        // GET /api/courses — list all courses (authenticated)
        CROW_ROUTE(app, "/api/courses").methods("GET"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }

            std::string search = "";
            auto search_param = req.url_params.get("search");
            if (search_param) search = search_param;

            std::vector<Course> courses;
            if (!search.empty()) {
                courses = course_service_.search_courses(search);
            } else {
                courses = course_service_.get_all_courses();
            }

            nlohmann::json courses_json = nlohmann::json::array();
            for (const auto& c : courses) {
                courses_json.push_back(c.to_json());
            }

            auto resp = crow::response(200, success_response(courses_json).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // GET /api/courses/<int> — get single course
        CROW_ROUTE(app, "/api/courses/<int>").methods("GET"_method)
        ([this](const crow::request& req, int id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }

            auto course = course_service_.get_course(id);
            if (!course) {
                return crow::response(404, error_response("Course not found", 404).dump());
            }

            auto resp = crow::response(200, success_response(course->to_json()).dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });

        // POST /api/courses — create course (faculty/admin)
        CROW_ROUTE(app, "/api/courses").methods("POST"_method)
        ([this](const crow::request& req) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role != Role::Faculty && user->role != Role::Admin) {
                return crow::response(403, error_response("Forbidden", 403).dump());
            }

            try {
                auto body = nlohmann::json::parse(req.body);
                Course course = Course::from_json(body);
                if (user->role == Role::Faculty) {
                    course.faculty_id = user->id;
                }

                auto result = course_service_.create_course(course, *user);
                if (!result.success) {
                    return crow::response(400, error_response(result.error).dump());
                }

                auto created = course_service_.get_course(result.course_id);
                auto resp = crow::response(201, success_response(
                    created ? created->to_json() : nlohmann::json{}, "Course created").dump());
                resp.add_header("Content-Type", "application/json");
                return resp;
            } catch (const nlohmann::json::exception&) {
                return crow::response(400, error_response("Invalid JSON body").dump());
            }
        });

        // PUT /api/courses/<int> — update course
        CROW_ROUTE(app, "/api/courses/<int>").methods("PUT"_method)
        ([this](const crow::request& req, int id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }
            if (user->role != Role::Faculty && user->role != Role::Admin) {
                return crow::response(403, error_response("Forbidden", 403).dump());
            }

            try {
                auto body = nlohmann::json::parse(req.body);
                Course course = Course::from_json(body);
                course.id = id;

                auto result = course_service_.update_course(course, *user);
                if (!result.success) {
                    return crow::response(400, error_response(result.error).dump());
                }

                auto updated = course_service_.get_course(id);
                auto resp = crow::response(200, success_response(
                    updated ? updated->to_json() : nlohmann::json{}, "Course updated").dump());
                resp.add_header("Content-Type", "application/json");
                return resp;
            } catch (const nlohmann::json::exception&) {
                return crow::response(400, error_response("Invalid JSON body").dump());
            }
        });

        // DELETE /api/courses/<int> — delete course (admin/faculty owner)
        CROW_ROUTE(app, "/api/courses/<int>").methods("DELETE"_method)
        ([this](const crow::request& req, int id) {
            auto user = authenticate_request(req.get_header_value("Authorization"), auth_service_);
            if (!user) {
                return crow::response(401, error_response("Not authenticated", 401).dump());
            }

            auto result = course_service_.delete_course(id, *user);
            if (!result.success) {
                return crow::response(400, error_response(result.error).dump());
            }

            auto resp = crow::response(200, success_response(nullptr, "Course deleted").dump());
            resp.add_header("Content-Type", "application/json");
            return resp;
        });
    }

private:
    CourseService& course_service_;
    AuthService& auth_service_;
};

} // namespace academia
