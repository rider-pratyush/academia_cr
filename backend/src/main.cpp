/**
 * main.cpp — Academia Course Registration Server
 * 
 * Entry point for the C++20 REST API server.
 * Wires together all components using dependency injection (constructor injection).
 * 
 * ARCHITECTURE:
 *   main() → Database → Repositories → Services → Controllers → Crow Routes
 * 
 * OS CONCEPTS:
 *   - Signal handling: graceful shutdown on SIGINT/SIGTERM
 *   - Thread pool: Crow uses internal thread pool for request handling
 *   - RAII: all resources cleaned up automatically via destructors
 */

#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"
#include "repositories/CourseRepository.hpp"
#include "repositories/RegistrationRepository.hpp"
#include "services/AuthService.hpp"
#include "services/CourseService.hpp"
#include "services/RegistrationService.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/CourseController.hpp"
#include "controllers/RegistrationController.hpp"
#include "controllers/AdminController.hpp"
#include "concurrency/ThreadPool.hpp"
#include "utils/Logger.hpp"

#include <crow.h>
#include <csignal>
#include <cstdlib>
#include <iostream>

// Global app pointer for signal handling
static crow::SimpleApp* g_app = nullptr;

void signal_handler(int sig) {
    LOG_INFO("Received signal " + std::to_string(sig) + ", shutting down...");
    if (g_app) {
        g_app->stop();
    }
}

int main(int argc, char* argv[]) {
    // --- Configuration from environment variables ---
    const char* db_path_env = std::getenv("ACADEMIA_DB_PATH");
    const char* port_env = std::getenv("ACADEMIA_PORT");
    const char* log_file_env = std::getenv("ACADEMIA_LOG_FILE");
    const char* threads_env = std::getenv("ACADEMIA_THREADS");

    std::string db_path = db_path_env ? db_path_env : "academia.db";
    int port = port_env ? std::atoi(port_env) : 8080;
    std::string log_file = log_file_env ? log_file_env : "academia_server.log";
    int num_threads = threads_env ? std::atoi(threads_env) : 4;

    // --- Setup logging ---
    auto& logger = academia::Logger::instance();
    logger.set_file(log_file);
    logger.set_level(academia::LogLevel::INFO);

    LOG_INFO("=== Academia Course Registration Server ===");
    LOG_INFO("Database: " + db_path);
    LOG_INFO("Port: " + std::to_string(port));
    LOG_INFO("Threads: " + std::to_string(num_threads));

    try {
        // --- Initialize database (RAII — auto-closed on scope exit) ---
        academia::Database db(db_path);
        db.initialize_schema();
        db.seed_data();

        // --- Create repositories ---
        academia::UserRepository user_repo(db);
        academia::CourseRepository course_repo(db);
        academia::RegistrationRepository reg_repo(db);

        // --- Create services ---
        academia::AuthService auth_service(user_repo);
        academia::CourseService course_service(course_repo, reg_repo);
        academia::RegistrationService reg_service(db, course_repo, reg_repo);

        // --- Create controllers ---
        academia::AuthController auth_ctrl(auth_service);
        academia::CourseController course_ctrl(course_service, auth_service);
        academia::RegistrationController reg_ctrl(reg_service, auth_service, reg_repo);
        academia::AdminController admin_ctrl(auth_service, user_repo, course_repo, reg_repo);

        // --- Setup Crow app ---
        crow::SimpleApp app;
        g_app = &app;

        // CORS: Manually add headers since SimpleApp doesn't support CORSHandler middleware.
        // We handle preflight OPTIONS and add CORS headers via after_handle hook.

        // Add CORS headers to all responses via a catch-all OPTIONS handler
        CROW_ROUTE(app, "/api/<path>").methods("OPTIONS"_method)
        ([](const crow::request&, std::string) {
            crow::response resp(204);
            resp.add_header("Access-Control-Allow-Origin", "*");
            resp.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            resp.add_header("Access-Control-Max-Age", "86400");
            return resp;
        });

        // Register routes
        auth_ctrl.register_routes(app);
        course_ctrl.register_routes(app);
        reg_ctrl.register_routes(app);
        admin_ctrl.register_routes(app);

        // Health check endpoint
        CROW_ROUTE(app, "/api/health").methods("GET"_method)
        ([](const crow::request&) {
            nlohmann::json health = {
                {"status", "healthy"},
                {"service", "Academia Course Registration API"},
                {"version", "1.0.0"}
            };
            crow::response resp(200, health.dump());
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

        // Setup signal handling for graceful shutdown
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // NOTE: CORS is handled via:
        //  - OPTIONS preflight route above
        //  - Vite dev server proxy in development (same origin)
        //  - nginx reverse proxy in production (same origin)

        LOG_INFO("Server starting on port " + std::to_string(port));
        std::cout << "\n"
                  << "╔══════════════════════════════════════════════╗\n"
                  << "║   Academia Course Registration Server v1.0   ║\n"
                  << "║   Port: " << port << "                                  ║\n"
                  << "║   Threads: " << num_threads << "                                ║\n"
                  << "║   Database: " << db_path << "                    ║\n"
                  << "╚══════════════════════════════════════════════╝\n"
                  << "\n"
                  << "Demo Accounts:\n"
                  << "  Admin:   admin / admin123\n"
                  << "  Faculty: dr.smith / faculty123\n"
                  << "  Student: john.doe / student123\n"
                  << "\n"
                  << "API: http://localhost:" << port << "/api/health\n"
                  << std::endl;

        app.port(port)
           .concurrency(num_threads)
           .run();

        LOG_INFO("Server shutdown complete");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Fatal error: ") + e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
