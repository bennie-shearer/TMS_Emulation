/**
 * @file logger.h
 * @brief TMS Tape Management System - Logging Framework
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_LOGGER_H
#define TMS_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <functional>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <vector>
#include <map>

namespace tms {

/**
 * @brief Thread-safe logging with file rotation and color support
 */
class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        LOG_ERROR = 4,
        CRITICAL = 5,
        OFF = 6
    };
    
    using LogCallback = std::function<void(Level, const std::string&, const std::string&)>;
    
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    void set_level(Level level) { min_level_ = level; }
    Level get_level() const { return min_level_; }
    
    void enable_console(bool enable) { console_enabled_ = enable; }
    void enable_colors(bool enable) { colors_enabled_ = enable; }
    bool is_console_enabled() const { return console_enabled_; }
    bool are_colors_enabled() const { return colors_enabled_; }
    
    bool set_log_file(const std::string& path, size_t max_size = 10 * 1024 * 1024,
                      size_t max_files = 5);
    void close_log_file();
    bool is_file_logging_enabled() const { return file_.is_open(); }
    
    void set_callback(LogCallback callback) { callback_ = callback; }
    
    void log(Level level, const std::string& component, const std::string& message);
    
    template<typename... Args>
    void logf(Level level, const std::string& component, 
              const std::string& format, Args&&... args) {
        if (level < min_level_) return;
        std::ostringstream oss;
        format_impl(oss, format, std::forward<Args>(args)...);
        log(level, component, oss.str());
    }
    
    void trace(const std::string& comp, const std::string& msg) { log(Level::TRACE, comp, msg); }
    void debug(const std::string& comp, const std::string& msg) { log(Level::DEBUG, comp, msg); }
    void info(const std::string& comp, const std::string& msg) { log(Level::INFO, comp, msg); }
    void warning(const std::string& comp, const std::string& msg) { log(Level::WARNING, comp, msg); }
    void error(const std::string& comp, const std::string& msg) { log(Level::LOG_ERROR, comp, msg); }
    void critical(const std::string& comp, const std::string& msg) { log(Level::CRITICAL, comp, msg); }
    
    size_t get_log_count() const { return log_count_.load(); }
    size_t get_error_count() const { return error_count_.load(); }
    size_t get_warning_count() const { return warning_count_.load(); }
    
    void reset_counters() {
        log_count_ = 0;
        error_count_ = 0;
        warning_count_ = 0;
    }
    
    static std::string level_to_string(Level level);
    static Level string_to_level(const std::string& str);
    
    std::string get_statistics() const;
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void rotate_logs();
    std::string get_color_code(Level level) const;
    std::string get_timestamp() const;
    
    void format_impl(std::ostringstream& oss, const std::string& fmt) {
        oss << fmt;
    }
    
    template<typename T>
    void format_impl(std::ostringstream& oss, const std::string& fmt, T&& arg) {
        size_t pos = fmt.find("{}");
        if (pos != std::string::npos) {
            oss << fmt.substr(0, pos) << arg << fmt.substr(pos + 2);
        } else {
            oss << fmt;
        }
    }
    
    template<typename T, typename... Rest>
    void format_impl(std::ostringstream& oss, const std::string& fmt, T&& arg, Rest&&... rest) {
        size_t pos = fmt.find("{}");
        if (pos != std::string::npos) {
            std::ostringstream temp;
            temp << fmt.substr(0, pos) << arg;
            format_impl(oss, temp.str() + fmt.substr(pos + 2), std::forward<Rest>(rest)...);
        } else {
            oss << fmt;
        }
    }
    
    std::mutex mutex_;
    std::ofstream file_;
    std::string file_path_;
    size_t max_file_size_ = 10 * 1024 * 1024;
    size_t max_files_ = 5;
    size_t current_file_size_ = 0;
    
    Level min_level_ = Level::INFO;
    bool console_enabled_ = true;
    bool colors_enabled_ = true;
    LogCallback callback_;
    
    std::atomic<size_t> log_count_{0};
    std::atomic<size_t> error_count_{0};
    std::atomic<size_t> warning_count_{0};
    
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief RAII timer for performance logging
 */
class ScopedLogTimer {
public:
    ScopedLogTimer(const std::string& component, const std::string& operation,
                   Logger::Level level = Logger::Level::DEBUG)
        : component_(component), operation_(operation), level_(level),
          start_(std::chrono::steady_clock::now()) {}
    
    ~ScopedLogTimer() {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        Logger::instance().log(level_, component_, 
            operation_ + " completed in " + std::to_string(ms) + "ms");
    }
    
    long long elapsed_ms() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }
    
private:
    std::string component_;
    std::string operation_;
    Logger::Level level_;
    std::chrono::steady_clock::time_point start_;
};

/**
 * @brief Performance metrics collector
 */
class PerformanceMetrics {
public:
    static PerformanceMetrics& instance() {
        static PerformanceMetrics metrics;
        return metrics;
    }
    
    void record_operation(const std::string& operation, long long duration_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = operations_[operation];
        stats.count++;
        stats.total_ms += duration_ms;
        if (duration_ms < stats.min_ms || stats.min_ms == 0) stats.min_ms = duration_ms;
        if (duration_ms > stats.max_ms) stats.max_ms = duration_ms;
    }
    
    void increment_counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_[name]++;
    }
    
    void set_gauge(const std::string& name, long long value) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] = value;
    }
    
    std::string get_report() const;
    void reset();
    
private:
    PerformanceMetrics() = default;
    
    struct OperationStats {
        size_t count = 0;
        long long total_ms = 0;
        long long min_ms = 0;
        long long max_ms = 0;
        
        double avg_ms() const { return count > 0 ? static_cast<double>(total_ms) / count : 0; }
    };
    
    mutable std::mutex mutex_;
    std::map<std::string, OperationStats> operations_;
    std::map<std::string, size_t> counters_;
    std::map<std::string, long long> gauges_;
};

// Note: get_timestamp() is defined in tms_utils.h

// Convenience macros
#define TMS_LOG_TRACE(comp, msg) tms::Logger::instance().trace(comp, msg)
#define TMS_LOG_DEBUG(comp, msg) tms::Logger::instance().debug(comp, msg)
#define TMS_LOG_INFO(comp, msg) tms::Logger::instance().info(comp, msg)
#define TMS_LOG_WARNING(comp, msg) tms::Logger::instance().warning(comp, msg)
#define TMS_LOG_ERROR(comp, msg) tms::Logger::instance().error(comp, msg)
#define TMS_LOG_CRITICAL(comp, msg) tms::Logger::instance().critical(comp, msg)

#define TMS_SCOPED_TIMER(comp, op) tms::ScopedLogTimer _timer_##__LINE__(comp, op)

} // namespace tms

#endif // TMS_LOGGER_H
