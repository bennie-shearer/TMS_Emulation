/**
 * @file logger.cpp
 * @brief TMS Tape Management System - Logger Implementation
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#include "logger.h"
#include <iostream>
#include <filesystem>
#include <map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace tms {

Logger::Logger() : start_time_(std::chrono::steady_clock::now()) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

Logger::~Logger() {
    close_log_file();
}

bool Logger::set_log_file(const std::string& path, size_t max_size, size_t max_files) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_.is_open()) {
        file_.close();
    }
    
    file_path_ = path;
    max_file_size_ = max_size;
    max_files_ = max_files;
    
    // Create parent directories if needed
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {
        // Ignore directory creation errors
    }
    
    file_.open(path, std::ios::app);
    if (!file_.is_open()) {
        return false;
    }
    
    file_.seekp(0, std::ios::end);
    current_file_size_ = static_cast<size_t>(file_.tellp());
    
    return true;
}

void Logger::close_log_file() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::rotate_logs() {
    if (!file_.is_open() || file_path_.empty()) return;
    
    file_.close();
    
    namespace fs = std::filesystem;
    
    // Remove oldest log if we've hit the limit
    for (size_t i = max_files_ - 1; i > 0; --i) {
        std::string old_name = file_path_ + "." + std::to_string(i);
        std::string new_name = file_path_ + "." + std::to_string(i + 1);
        
        try {
            if (fs::exists(old_name)) {
                if (i == max_files_ - 1) {
                    fs::remove(old_name);
                } else {
                    fs::rename(old_name, new_name);
                }
            }
        } catch (...) {
            // Ignore rotation errors
        }
    }
    
    try {
        if (fs::exists(file_path_)) {
            fs::rename(file_path_, file_path_ + ".1");
        }
    } catch (...) {
        // Ignore
    }
    
    file_.open(file_path_, std::ios::out);
    current_file_size_ = 0;
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::get_color_code(Level level) const {
    if (!colors_enabled_) return "";
    
    switch (level) {
        case Level::TRACE:     return "\033[90m";      // Dark gray
        case Level::DEBUG:     return "\033[36m";      // Cyan
        case Level::INFO:      return "\033[32m";      // Green
        case Level::WARNING:   return "\033[33m";      // Yellow
        case Level::LOG_ERROR: return "\033[31m";      // Red
        case Level::CRITICAL:  return "\033[35;1m";    // Bright magenta
        default: return "";
    }
}

void Logger::log(Level level, const std::string& component, const std::string& message) {
    if (level < min_level_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    log_count_++;
    if (level >= Level::LOG_ERROR) {
        error_count_++;
    } else if (level == Level::WARNING) {
        warning_count_++;
    }
    
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    
    // Truncate component to 12 chars for alignment
    std::string comp = component.length() > 12 ? component.substr(0, 12) : component;
    
    std::ostringstream formatted;
    formatted << timestamp << " [" << std::setw(8) << level_str << "] "
              << "[" << std::setw(12) << comp << "] " << message;
    
    if (console_enabled_) {
        std::string color = get_color_code(level);
        std::string reset = colors_enabled_ ? "\033[0m" : "";
        
        // Use cerr for errors and above
        if (level >= Level::LOG_ERROR) {
            std::cerr << color << formatted.str() << reset << "\n";
        } else {
            std::cout << color << formatted.str() << reset << "\n";
        }
    }
    
    if (file_.is_open()) {
        file_ << formatted.str() << "\n";
        file_.flush();
        current_file_size_ += formatted.str().size() + 1;
        
        if (current_file_size_ >= max_file_size_) {
            rotate_logs();
        }
    }
    
    if (callback_) {
        callback_(level, component, message);
    }
}

std::string Logger::level_to_string(Level level) {
    switch (level) {
        case Level::TRACE:     return "TRACE";
        case Level::DEBUG:     return "DEBUG";
        case Level::INFO:      return "INFO";
        case Level::WARNING:   return "WARNING";
        case Level::LOG_ERROR: return "ERROR";
        case Level::CRITICAL:  return "CRITICAL";
        case Level::OFF:       return "OFF";
        default: return "UNKNOWN";
    }
}

Logger::Level Logger::string_to_level(const std::string& str) {
    if (str == "TRACE") return Level::TRACE;
    if (str == "DEBUG") return Level::DEBUG;
    if (str == "INFO") return Level::INFO;
    if (str == "WARNING") return Level::WARNING;
    if (str == "ERROR") return Level::LOG_ERROR;
    if (str == "CRITICAL") return Level::CRITICAL;
    if (str == "OFF") return Level::OFF;
    return Level::INFO;
}

std::string Logger::get_statistics() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    std::ostringstream oss;
    oss << "Logger Statistics:\n"
        << "  Total logs: " << log_count_.load() << "\n"
        << "  Warnings: " << warning_count_.load() << "\n"
        << "  Errors: " << error_count_.load() << "\n"
        << "  Uptime: " << uptime.count() << " seconds\n"
        << "  Console: " << (console_enabled_ ? "enabled" : "disabled") << "\n"
        << "  File: " << (file_.is_open() ? file_path_ : "disabled") << "\n";
    return oss.str();
}

// ============================================================================
// Performance Metrics
// ============================================================================

std::string PerformanceMetrics::get_report() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "\n=== PERFORMANCE METRICS ===\n\n";
    
    if (!operations_.empty()) {
        oss << "Operations:\n";
        oss << std::left << std::setw(25) << "  Operation"
            << std::setw(10) << "Count"
            << std::setw(12) << "Avg(ms)"
            << std::setw(12) << "Min(ms)"
            << std::setw(12) << "Max(ms)" << "\n";
        oss << std::string(71, '-') << "\n";
        
        for (const auto& [name, stats] : operations_) {
            oss << std::left << std::setw(25) << ("  " + name.substr(0, 22))
                << std::setw(10) << stats.count
                << std::fixed << std::setprecision(1)
                << std::setw(12) << stats.avg_ms()
                << std::setw(12) << stats.min_ms
                << std::setw(12) << stats.max_ms << "\n";
        }
    }
    
    if (!counters_.empty()) {
        oss << "\nCounters:\n";
        for (const auto& [name, value] : counters_) {
            oss << "  " << name << ": " << value << "\n";
        }
    }
    
    if (!gauges_.empty()) {
        oss << "\nGauges:\n";
        for (const auto& [name, value] : gauges_) {
            oss << "  " << name << ": " << value << "\n";
        }
    }
    
    return oss.str();
}

void PerformanceMetrics::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    operations_.clear();
    counters_.clear();
    gauges_.clear();
}

} // namespace tms
