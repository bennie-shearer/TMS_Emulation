/**
 * @file error_codes.h
 * @brief TMS Tape Management System - Error Code Definitions
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_ERROR_CODES_H
#define TMS_ERROR_CODES_H

#include <string>
#include <variant>
#include <functional>

// Provide fallback for compilers without full C++20 source_location support
#if __has_include(<source_location>) && defined(__cpp_lib_source_location)
    #include <source_location>
    #define TMS_HAS_SOURCE_LOCATION 1
#else
    #define TMS_HAS_SOURCE_LOCATION 0
#endif

namespace tms {

/**
 * @brief Error codes for TMS operations
 */
enum class TMSError {
    SUCCESS = 0,
    
    // Volume errors (100-199)
    VOLUME_NOT_FOUND = 100,
    VOLUME_ALREADY_EXISTS = 101,
    VOLUME_IN_USE = 102,
    VOLUME_MOUNTED = 103,
    VOLUME_NOT_MOUNTED = 104,
    VOLUME_WRITE_PROTECTED = 105,
    VOLUME_EXPIRED = 106,
    VOLUME_HAS_DATASETS = 107,
    VOLUME_OFFLINE = 108,
    VOLUME_ERROR_STATE = 109,
    VOLUME_RESERVED = 110,
    VOLUME_RESERVATION_EXPIRED = 111,
    VOLUME_LIMIT_REACHED = 112,
    
    // Dataset errors (200-299)
    DATASET_NOT_FOUND = 200,
    DATASET_ALREADY_EXISTS = 201,
    DATASET_MIGRATED = 202,
    DATASET_EXPIRED = 203,
    DATASET_ACTIVE = 204,
    DATASET_ON_DIFFERENT_VOLUME = 205,
    DATASET_LIMIT_REACHED = 206,
    DATASET_NAME_CONFLICT = 207,
    
    // Operation errors (300-399)
    OPERATION_FAILED = 300,
    OPERATION_TIMEOUT = 301,
    OPERATION_CANCELLED = 302,
    OPERATION_NOT_SUPPORTED = 303,
    LOCK_TIMEOUT = 304,
    CONCURRENT_MODIFICATION = 305,
    BATCH_PARTIAL_FAILURE = 306,
    RETRY_EXHAUSTED = 307,
    
    // I/O errors (400-499)
    FILE_NOT_FOUND = 400,
    FILE_EXISTS = 401,
    FILE_OPEN_ERROR = 402,
    FILE_READ_ERROR = 403,
    FILE_WRITE_ERROR = 404,
    FILE_PERMISSION_DENIED = 405,
    DIRECTORY_NOT_FOUND = 406,
    DIRECTORY_CREATE_FAILED = 407,
    FILE_FORMAT_ERROR = 408,
    FILE_CORRUPTED = 409,
    
    // System errors (500-599)
    OUT_OF_MEMORY = 500,
    SYSTEM_ERROR = 501,
    CONFIGURATION_ERROR = 502,
    INITIALIZATION_FAILED = 503,
    SHUTDOWN_ERROR = 504,
    NOT_IMPLEMENTED = 505,
    INTERNAL_ERROR = 506,
    
    // Validation errors (600-699)
    INVALID_VOLSER = 600,
    INVALID_DATASET_NAME = 601,
    INVALID_PARAMETER = 602,
    INVALID_STATE = 603,
    INVALID_FORMAT = 604,
    VALIDATION_FAILED = 605,
    NAME_TOO_LONG = 606,
    EMPTY_NAME = 607,
    INVALID_TAG = 608,
    TOO_MANY_TAGS = 609,
    INVALID_DATE = 610,
    INVALID_SIZE = 611,
    
    // Security errors (700-799)
    ACCESS_DENIED = 700,
    AUTHENTICATION_FAILED = 701,
    AUTHORIZATION_FAILED = 702,
    PERMISSION_DENIED = 703,
    
    // Audit errors (800-899)
    AUDIT_LOG_FULL = 800,
    AUDIT_WRITE_FAILED = 801,
    AUDIT_READ_FAILED = 802,
    
    // Scratch pool errors (900-999)
    NO_SCRATCH_AVAILABLE = 900,
    POOL_NOT_FOUND = 901,
    POOL_EMPTY = 902,
    POOL_EXHAUSTED = 903,
    
    UNKNOWN_ERROR = 9999
};

/**
 * @brief Error information with optional source location tracking
 */
struct ErrorInfo {
    TMSError code = TMSError::SUCCESS;
    std::string message;
    std::string file;
    int line = 0;
    std::string function;
    
    ErrorInfo() = default;
    
#if TMS_HAS_SOURCE_LOCATION
    ErrorInfo(TMSError c, const std::string& msg,
              const std::source_location& loc = std::source_location::current())
        : code(c), message(msg), file(loc.file_name()),
          line(static_cast<int>(loc.line())), function(loc.function_name()) {}
#else
    ErrorInfo(TMSError c, const std::string& msg,
              const char* file_name = "", int line_num = 0, const char* func_name = "")
        : code(c), message(msg), file(file_name),
          line(line_num), function(func_name) {}
#endif
    
    explicit operator bool() const { return code == TMSError::SUCCESS; }
    bool is_error() const { return code != TMSError::SUCCESS; }
    
    std::string to_string() const {
        std::string result = message;
        if (!file.empty()) {
            result += " [" + file + ":" + std::to_string(line) + "]";
        }
        return result;
    }
};

/**
 * @brief Result type for operations that return a value
 */
template<typename T>
class Result {
public:
    using ValueType = T;
    
    static Result ok(const T& value) {
        Result r;
        r.data_ = value;
        return r;
    }
    
    static Result ok(T&& value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }
    
#if TMS_HAS_SOURCE_LOCATION
    static Result err(TMSError code, const std::string& message,
                      const std::source_location& loc = std::source_location::current()) {
        Result r;
        r.data_ = ErrorInfo(code, message, loc);
        return r;
    }
#else
    static Result err(TMSError code, const std::string& message) {
        Result r;
        r.data_ = ErrorInfo(code, message);
        return r;
    }
#endif
    
    bool is_success() const { return std::holds_alternative<T>(data_); }
    bool is_error() const { return std::holds_alternative<ErrorInfo>(data_); }
    explicit operator bool() const { return is_success(); }
    
    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }
    
    const T& value_or(const T& default_value) const {
        return is_success() ? std::get<T>(data_) : default_value;
    }
    
    const ErrorInfo& error() const { return std::get<ErrorInfo>(data_); }
    TMSError error_code() const { return is_error() ? error().code : TMSError::SUCCESS; }
    
    template<typename Func>
    auto map(Func&& func) const -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        if (is_success()) {
            return Result<U>::ok(func(value()));
        }
        return Result<U>::err(error().code, error().message);
    }
    
private:
    std::variant<T, ErrorInfo> data_;
};

/**
 * @brief Result type for void operations
 */
class OperationResult {
public:
    static OperationResult ok() {
        return OperationResult();
    }
    
#if TMS_HAS_SOURCE_LOCATION
    static OperationResult err(TMSError code, const std::string& message,
                               const std::source_location& loc = std::source_location::current()) {
        OperationResult r;
        r.error_ = ErrorInfo(code, message, loc);
        return r;
    }
#else
    static OperationResult err(TMSError code, const std::string& message) {
        OperationResult r;
        r.error_ = ErrorInfo(code, message);
        return r;
    }
#endif
    
    bool is_success() const { return error_.code == TMSError::SUCCESS; }
    bool is_error() const { return error_.code != TMSError::SUCCESS; }
    explicit operator bool() const { return is_success(); }
    
    const ErrorInfo& error() const { return error_; }
    TMSError error_code() const { return error_.code; }
    
private:
    ErrorInfo error_;
};

// Utility functions
inline std::string error_to_string(TMSError code) {
    switch (code) {
        case TMSError::SUCCESS: return "Success";
        case TMSError::VOLUME_NOT_FOUND: return "Volume not found";
        case TMSError::VOLUME_ALREADY_EXISTS: return "Volume already exists";
        case TMSError::VOLUME_IN_USE: return "Volume in use";
        case TMSError::VOLUME_MOUNTED: return "Volume is mounted";
        case TMSError::VOLUME_NOT_MOUNTED: return "Volume not mounted";
        case TMSError::VOLUME_WRITE_PROTECTED: return "Volume is write protected";
        case TMSError::VOLUME_EXPIRED: return "Volume has expired";
        case TMSError::VOLUME_HAS_DATASETS: return "Volume has datasets";
        case TMSError::VOLUME_RESERVED: return "Volume is reserved";
        case TMSError::VOLUME_RESERVATION_EXPIRED: return "Volume reservation expired";
        case TMSError::VOLUME_LIMIT_REACHED: return "Volume limit reached";
        case TMSError::DATASET_NOT_FOUND: return "Dataset not found";
        case TMSError::DATASET_ALREADY_EXISTS: return "Dataset already exists";
        case TMSError::DATASET_MIGRATED: return "Dataset is migrated";
        case TMSError::DATASET_LIMIT_REACHED: return "Dataset limit reached";
        case TMSError::NO_SCRATCH_AVAILABLE: return "No scratch volumes available";
        case TMSError::INVALID_VOLSER: return "Invalid volume serial";
        case TMSError::INVALID_DATASET_NAME: return "Invalid dataset name";
        case TMSError::INVALID_PARAMETER: return "Invalid parameter";
        case TMSError::INVALID_TAG: return "Invalid tag";
        case TMSError::TOO_MANY_TAGS: return "Too many tags";
        case TMSError::FILE_OPEN_ERROR: return "File open error";
        case TMSError::FILE_WRITE_ERROR: return "File write error";
        case TMSError::FILE_READ_ERROR: return "File read error";
        case TMSError::FILE_FORMAT_ERROR: return "File format error";
        case TMSError::NOT_IMPLEMENTED: return "Not implemented";
        case TMSError::OPERATION_TIMEOUT: return "Operation timeout";
        case TMSError::LOCK_TIMEOUT: return "Lock timeout";
        case TMSError::RETRY_EXHAUSTED: return "Retry attempts exhausted";
        case TMSError::BATCH_PARTIAL_FAILURE: return "Batch operation partial failure";
        default: return "Unknown error";
    }
}

inline std::string get_error_category(TMSError code) {
    int c = static_cast<int>(code);
    if (c == 0) return "Success";
    if (c >= 100 && c < 200) return "Volume";
    if (c >= 200 && c < 300) return "Dataset";
    if (c >= 300 && c < 400) return "Operation";
    if (c >= 400 && c < 500) return "I/O";
    if (c >= 500 && c < 600) return "System";
    if (c >= 600 && c < 700) return "Validation";
    if (c >= 700 && c < 800) return "Security";
    if (c >= 800 && c < 900) return "Audit";
    if (c >= 900 && c < 1000) return "Scratch";
    return "Unknown";
}

inline bool is_recoverable_error(TMSError code) {
    switch (code) {
        case TMSError::LOCK_TIMEOUT:
        case TMSError::OPERATION_TIMEOUT:
        case TMSError::FILE_OPEN_ERROR:
        case TMSError::CONCURRENT_MODIFICATION:
            return true;
        default:
            return false;
    }
}

inline bool is_transient_error(TMSError code) {
    switch (code) {
        case TMSError::LOCK_TIMEOUT:
        case TMSError::OPERATION_TIMEOUT:
        case TMSError::FILE_OPEN_ERROR:
            return true;
        default:
            return false;
    }
}

inline int get_error_severity(TMSError code) {
    // 1 = Info, 2 = Warning, 3 = Error, 4 = Critical
    switch (code) {
        case TMSError::SUCCESS:
            return 1;
        case TMSError::VOLUME_RESERVED:
        case TMSError::DATASET_MIGRATED:
            return 2;
        case TMSError::VOLUME_NOT_FOUND:
        case TMSError::DATASET_NOT_FOUND:
        case TMSError::INVALID_PARAMETER:
            return 3;
        case TMSError::FILE_CORRUPTED:
        case TMSError::INTERNAL_ERROR:
        case TMSError::OUT_OF_MEMORY:
            return 4;
        default:
            return 3;
    }
}

} // namespace tms

#endif // TMS_ERROR_CODES_H
