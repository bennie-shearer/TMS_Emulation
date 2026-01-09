/**
 * @file tms_backup.h
 * @brief TMS Tape Management System - Backup Rotation Management
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides automatic backup rotation with configurable retention policies
 * supporting daily, weekly, and monthly rotation schemes.
 */

#ifndef TMS_BACKUP_H
#define TMS_BACKUP_H

#include "tms_utils.h"
#include "error_codes.h"
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <regex>

namespace tms {

namespace fs = std::filesystem;

// ============================================================================
// Backup Types
// ============================================================================

/**
 * @brief Backup rotation scheme
 */
enum class RotationScheme {
    SIMPLE,         ///< Keep N most recent backups
    DAILY,          ///< Daily backups with retention period
    GFS             ///< Grandfather-Father-Son (daily/weekly/monthly)
};

/**
 * @brief Backup metadata
 */
struct BackupInfo {
    std::string filename;
    std::string full_path;
    std::chrono::system_clock::time_point timestamp;
    uint64_t size_bytes = 0;
    std::string type;  // "daily", "weekly", "monthly", "manual"
    bool verified = false;
    std::string checksum;
    
    bool is_expired(std::chrono::hours max_age) const {
        auto age = std::chrono::system_clock::now() - timestamp;
        return age > max_age;
    }
};

/**
 * @brief Backup configuration
 */
struct BackupConfig {
    std::string backup_directory = "backups";
    std::string backup_prefix = "tms_backup";
    RotationScheme scheme = RotationScheme::SIMPLE;
    
    // Simple rotation
    int keep_count = 10;                    ///< Number of backups to keep
    
    // Daily rotation
    int daily_retention_days = 7;           ///< Days to keep daily backups
    
    // GFS rotation
    int gfs_daily_count = 7;                ///< Daily backups to keep
    int gfs_weekly_count = 4;               ///< Weekly backups to keep
    int gfs_monthly_count = 12;             ///< Monthly backups to keep
    int gfs_weekly_day = 0;                 ///< Day for weekly (0=Sunday)
    int gfs_monthly_day = 1;                ///< Day for monthly (1-28)
    
    // Verification
    bool verify_backups = true;             ///< Verify backup integrity
    bool compress_backups = false;          ///< Compress backup files
};

/**
 * @brief Backup operation result
 */
struct BackupResult {
    bool success = false;
    std::string backup_path;
    std::string message;
    uint64_t size_bytes = 0;
    std::chrono::milliseconds duration{0};
    std::string checksum;
    int files_backed_up = 0;
};

/**
 * @brief Rotation result
 */
struct RotationResult {
    size_t backups_before = 0;
    size_t backups_after = 0;
    size_t deleted_count = 0;
    uint64_t space_freed = 0;
    std::vector<std::string> deleted_files;
    std::vector<std::string> errors;
};

// ============================================================================
// Backup Manager
// ============================================================================

/**
 * @brief Manages backup creation and rotation
 */
class BackupManager {
public:
    using BackupCallback = std::function<OperationResult(const std::string& path)>;
    
    explicit BackupManager(const BackupConfig& config = BackupConfig{});
    
    // Configuration
    void set_config(const BackupConfig& config);
    BackupConfig get_config() const;
    
    // Backup operations
    BackupResult create_backup(BackupCallback backup_fn, const std::string& type = "manual");
    BackupResult create_backup_from_files(const std::vector<std::string>& source_files,
                                          const std::string& type = "manual");
    
    // Rotation
    RotationResult rotate_backups();
    RotationResult cleanup_expired();
    
    // Queries
    std::vector<BackupInfo> list_backups() const;
    std::vector<BackupInfo> list_backups_by_type(const std::string& type) const;
    std::optional<BackupInfo> get_latest_backup() const;
    std::optional<BackupInfo> get_backup(const std::string& filename) const;
    
    // Verification
    bool verify_backup(const std::string& filename);
    std::vector<std::string> verify_all_backups();
    
    // Restoration
    OperationResult restore_backup(const std::string& filename, const std::string& dest_dir);
    
    // Maintenance
    OperationResult delete_backup(const std::string& filename);
    uint64_t get_total_backup_size() const;
    size_t get_backup_count() const;
    
    // Scheduling helpers
    bool should_create_daily_backup() const;
    bool should_create_weekly_backup() const;
    bool should_create_monthly_backup() const;
    
private:
    std::string generate_backup_filename(const std::string& type) const;
    std::string calculate_checksum(const std::string& path) const;
    std::vector<BackupInfo> scan_backup_directory() const;
    std::vector<BackupInfo> get_backups_to_delete() const;
    bool is_weekly_backup_day() const;
    bool is_monthly_backup_day() const;
    
    mutable std::mutex mutex_;
    BackupConfig config_;
    std::vector<BackupInfo> backup_cache_;
    std::chrono::system_clock::time_point last_scan_;
};

// ============================================================================
// Implementation
// ============================================================================

inline BackupManager::BackupManager(const BackupConfig& config) : config_(config) {
    if (!config_.backup_directory.empty()) {
        fs::create_directories(config_.backup_directory);
    }
}

inline void BackupManager::set_config(const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    if (!config_.backup_directory.empty()) {
        fs::create_directories(config_.backup_directory);
    }
}

inline BackupConfig BackupManager::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

inline BackupResult BackupManager::create_backup(BackupCallback backup_fn, const std::string& type) {
    auto start = std::chrono::steady_clock::now();
    BackupResult result;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filename = generate_backup_filename(type);
    std::string full_path = config_.backup_directory + PATH_SEP_STR + filename;
    
    auto op_result = backup_fn(full_path);
    
    if (op_result.is_success()) {
        result.success = true;
        result.backup_path = full_path;
        result.message = "Backup created successfully";
        
        if (fs::exists(full_path)) {
            result.size_bytes = fs::file_size(full_path);
            if (config_.verify_backups) {
                result.checksum = calculate_checksum(full_path);
            }
        }
        result.files_backed_up = 1;
    } else {
        result.success = false;
        result.message = op_result.error().message;
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Invalidate cache
    last_scan_ = std::chrono::system_clock::time_point{};
    
    return result;
}

inline BackupResult BackupManager::create_backup_from_files(
    const std::vector<std::string>& source_files,
    const std::string& type) {
    
    auto start = std::chrono::steady_clock::now();
    BackupResult result;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string filename = generate_backup_filename(type);
    std::string full_path = config_.backup_directory + PATH_SEP_STR + filename;
    
    try {
        // Create backup directory for this backup
        fs::create_directories(full_path);
        
        int count = 0;
        for (const auto& src : source_files) {
            if (fs::exists(src)) {
                std::string dest = full_path + PATH_SEP_STR + fs::path(src).filename().string();
                fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
                result.size_bytes += fs::file_size(dest);
                count++;
            }
        }
        
        result.success = true;
        result.backup_path = full_path;
        result.files_backed_up = count;
        result.message = "Backup created: " + std::to_string(count) + " files";
        
    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Backup failed: ") + e.what();
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    last_scan_ = std::chrono::system_clock::time_point{};
    
    return result;
}

inline RotationResult BackupManager::rotate_backups() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    RotationResult result;
    auto backups = scan_backup_directory();
    result.backups_before = backups.size();
    
    auto to_delete = get_backups_to_delete();
    
    for (const auto& backup : to_delete) {
        try {
            uint64_t size = 0;
            if (fs::exists(backup.full_path)) {
                if (fs::is_directory(backup.full_path)) {
                    for (const auto& entry : fs::recursive_directory_iterator(backup.full_path)) {
                        if (entry.is_regular_file()) {
                            size += entry.file_size();
                        }
                    }
                    fs::remove_all(backup.full_path);
                } else {
                    size = fs::file_size(backup.full_path);
                    fs::remove(backup.full_path);
                }
            }
            result.deleted_count++;
            result.space_freed += size;
            result.deleted_files.push_back(backup.filename);
        } catch (const std::exception& e) {
            result.errors.push_back(backup.filename + ": " + e.what());
        }
    }
    
    result.backups_after = result.backups_before - result.deleted_count;
    last_scan_ = std::chrono::system_clock::time_point{};
    
    return result;
}

inline std::vector<BackupInfo> BackupManager::list_backups() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scan_backup_directory();
}

inline std::vector<BackupInfo> BackupManager::list_backups_by_type(const std::string& type) const {
    auto all = list_backups();
    std::vector<BackupInfo> result;
    for (const auto& b : all) {
        if (b.type == type) {
            result.push_back(b);
        }
    }
    return result;
}

inline std::optional<BackupInfo> BackupManager::get_latest_backup() const {
    auto backups = list_backups();
    if (backups.empty()) {
        return std::nullopt;
    }
    
    auto latest = std::max_element(backups.begin(), backups.end(),
        [](const BackupInfo& a, const BackupInfo& b) {
            return a.timestamp < b.timestamp;
        });
    
    return *latest;
}

inline size_t BackupManager::get_backup_count() const {
    return list_backups().size();
}

inline uint64_t BackupManager::get_total_backup_size() const {
    uint64_t total = 0;
    for (const auto& b : list_backups()) {
        total += b.size_bytes;
    }
    return total;
}

inline bool BackupManager::should_create_daily_backup() const {
    auto latest = get_latest_backup();
    if (!latest.has_value()) return true;
    
    auto age = std::chrono::system_clock::now() - latest->timestamp;
    return age >= std::chrono::hours(24);
}

inline bool BackupManager::should_create_weekly_backup() const {
    auto weekly = list_backups_by_type("weekly");
    if (weekly.empty()) return is_weekly_backup_day();
    
    auto latest = std::max_element(weekly.begin(), weekly.end(),
        [](const BackupInfo& a, const BackupInfo& b) {
            return a.timestamp < b.timestamp;
        });
    
    auto age = std::chrono::system_clock::now() - latest->timestamp;
    return age >= std::chrono::hours(24 * 7) && is_weekly_backup_day();
}

inline bool BackupManager::should_create_monthly_backup() const {
    auto monthly = list_backups_by_type("monthly");
    if (monthly.empty()) return is_monthly_backup_day();
    
    auto latest = std::max_element(monthly.begin(), monthly.end(),
        [](const BackupInfo& a, const BackupInfo& b) {
            return a.timestamp < b.timestamp;
        });
    
    auto age = std::chrono::system_clock::now() - latest->timestamp;
    return age >= std::chrono::hours(24 * 28) && is_monthly_backup_day();
}

inline std::string BackupManager::generate_backup_filename(const std::string& type) const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << config_.backup_prefix << "_" << type << "_";
    oss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
    return oss.str();
}

inline std::vector<BackupInfo> BackupManager::scan_backup_directory() const {
    std::vector<BackupInfo> backups;
    
    if (!fs::exists(config_.backup_directory)) {
        return backups;
    }
    
    std::regex backup_pattern(config_.backup_prefix + R"(_(\w+)_(\d{8}_\d{6}))");
    
    for (const auto& entry : fs::directory_iterator(config_.backup_directory)) {
        BackupInfo info;
        info.filename = entry.path().filename().string();
        info.full_path = entry.path().string();
        
        std::smatch match;
        if (std::regex_search(info.filename, match, backup_pattern)) {
            info.type = match[1];
            
            // Parse timestamp
            std::string ts = match[2];
            std::tm tm = {};
            std::istringstream ss(ts);
            ss >> std::get_time(&tm, "%Y%m%d_%H%M%S");
            info.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
        
        if (entry.is_regular_file()) {
            info.size_bytes = entry.file_size();
        } else if (entry.is_directory()) {
            for (const auto& sub : fs::recursive_directory_iterator(entry)) {
                if (sub.is_regular_file()) {
                    info.size_bytes += sub.file_size();
                }
            }
        }
        
        backups.push_back(info);
    }
    
    // Sort by timestamp descending
    std::sort(backups.begin(), backups.end(),
        [](const BackupInfo& a, const BackupInfo& b) {
            return a.timestamp > b.timestamp;
        });
    
    return backups;
}

inline std::vector<BackupInfo> BackupManager::get_backups_to_delete() const {
    auto backups = scan_backup_directory();
    std::vector<BackupInfo> to_delete;
    
    switch (config_.scheme) {
        case RotationScheme::SIMPLE: {
            if (backups.size() > static_cast<size_t>(config_.keep_count)) {
                to_delete.insert(to_delete.end(),
                    backups.begin() + config_.keep_count, backups.end());
            }
            break;
        }
        
        case RotationScheme::DAILY: {
            auto threshold = std::chrono::system_clock::now() -
                std::chrono::hours(24 * config_.daily_retention_days);
            for (const auto& b : backups) {
                if (b.timestamp < threshold) {
                    to_delete.push_back(b);
                }
            }
            break;
        }
        
        case RotationScheme::GFS: {
            std::vector<BackupInfo> daily, weekly, monthly;
            for (const auto& b : backups) {
                if (b.type == "daily") daily.push_back(b);
                else if (b.type == "weekly") weekly.push_back(b);
                else if (b.type == "monthly") monthly.push_back(b);
            }
            
            if (daily.size() > static_cast<size_t>(config_.gfs_daily_count)) {
                to_delete.insert(to_delete.end(),
                    daily.begin() + config_.gfs_daily_count, daily.end());
            }
            if (weekly.size() > static_cast<size_t>(config_.gfs_weekly_count)) {
                to_delete.insert(to_delete.end(),
                    weekly.begin() + config_.gfs_weekly_count, weekly.end());
            }
            if (monthly.size() > static_cast<size_t>(config_.gfs_monthly_count)) {
                to_delete.insert(to_delete.end(),
                    monthly.begin() + config_.gfs_monthly_count, monthly.end());
            }
            break;
        }
    }
    
    return to_delete;
}

inline bool BackupManager::is_weekly_backup_day() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    return tm_buf.tm_wday == config_.gfs_weekly_day;
}

inline bool BackupManager::is_monthly_backup_day() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    return tm_buf.tm_mday == config_.gfs_monthly_day;
}

inline std::string BackupManager::calculate_checksum(const std::string& path) const {
    // Simple checksum (sum of bytes mod 2^32)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    uint32_t sum = 0;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        for (std::streamsize i = 0; i < file.gcount(); i++) {
            sum += static_cast<unsigned char>(buffer[i]);
        }
    }
    for (std::streamsize i = 0; i < file.gcount(); i++) {
        sum += static_cast<unsigned char>(buffer[i]);
    }
    
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << sum;
    return oss.str();
}

} // namespace tms

#endif // TMS_BACKUP_H
