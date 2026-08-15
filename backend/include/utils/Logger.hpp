#pragma once
/**
 * Logger.hpp — Thread-safe structured logging
 * 
 * OS CONCEPTS:
 *   - Uses std::mutex to serialize log writes from multiple threads
 *   - Demonstrates critical section protection for shared I/O resource
 *   - Lock guard (RAII) ensures the mutex is always released
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <string_view>

namespace academia {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) { min_level_ = level; }

    void set_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.close();
        file_.open(path, std::ios::app);
    }

    void log(LogLevel level, std::string_view message, 
             std::string_view file = "", int line = 0) {
        if (level < min_level_) return;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << level_string(level) << "] "
            << "[thread:" << std::this_thread::get_id() << "] ";

        if (!file.empty()) {
            // Extract just the filename from full path
            auto pos = file.find_last_of("/\\");
            auto fname = (pos != std::string_view::npos) ? file.substr(pos + 1) : file;
            oss << fname << ":" << line << " ";
        }

        oss << message << "\n";

        std::string output = oss.str();

        // CRITICAL SECTION: serialize output to prevent interleaved log lines
        // from different threads. Without this mutex, two threads writing
        // simultaneously could produce garbled output.
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << output;
        if (file_.is_open()) {
            file_ << output;
            file_.flush();
        }
    }

    void info(std::string_view msg, std::string_view file = "", int line = 0) {
        log(LogLevel::INFO, msg, file, line);
    }
    void warning(std::string_view msg, std::string_view file = "", int line = 0) {
        log(LogLevel::WARNING, msg, file, line);
    }
    void error(std::string_view msg, std::string_view file = "", int line = 0) {
        log(LogLevel::ERROR, msg, file, line);
    }
    void debug(std::string_view msg, std::string_view file = "", int line = 0) {
        log(LogLevel::DEBUG, msg, file, line);
    }

private:
    Logger() = default;
    ~Logger() { if (file_.is_open()) file_.close(); }

    static constexpr const char* level_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG  ";
            case LogLevel::INFO:    return "INFO   ";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR  ";
            default:                return "UNKNOWN";
        }
    }

    std::mutex mutex_;
    std::ofstream file_;
    LogLevel min_level_ = LogLevel::INFO;
};

// Convenience macros that capture file and line
#define LOG_INFO(msg)    academia::Logger::instance().info(msg, __FILE__, __LINE__)
#define LOG_WARN(msg)    academia::Logger::instance().warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg)   academia::Logger::instance().error(msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg)   academia::Logger::instance().debug(msg, __FILE__, __LINE__)

} // namespace academia
