/**
 * @file tms_version.h
 * @brief TMS Tape Management System - Version and Build Information
 * @version 3.3.0
 * @date 2026-01-09
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_VERSION_H
#define TMS_VERSION_H

#include <string>
#include <sstream>
#include <vector>

namespace tms {

// ============================================================================
// Version Information
// ============================================================================

constexpr int VERSION_MAJOR = 3;
constexpr int VERSION_MINOR = 3;
constexpr int VERSION_PATCH = 0;
constexpr const char* VERSION_STRING = "3.3.0";
constexpr const char* VERSION_DATE = "2026-01-09";
constexpr const char* VERSION_COPYRIGHT = "Copyright (c) 2025 Bennie Shearer";

// Catalog version for persistence compatibility
constexpr int CATALOG_VERSION = 8;
constexpr int MIN_CATALOG_VERSION = 1;

// ============================================================================
// Build Information
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    constexpr const char* PLATFORM_NAME = "Windows";
    constexpr const char PATH_SEP = '\\';
    constexpr const char* PATH_SEP_STR = "\\";
#elif defined(__APPLE__)
    constexpr const char* PLATFORM_NAME = "macOS";
    constexpr const char PATH_SEP = '/';
    constexpr const char* PATH_SEP_STR = "/";
#elif defined(__linux__)
    constexpr const char* PLATFORM_NAME = "Linux";
    constexpr const char PATH_SEP = '/';
    constexpr const char* PATH_SEP_STR = "/";
#else
    constexpr const char* PLATFORM_NAME = "Unknown";
    constexpr const char PATH_SEP = '/';
    constexpr const char* PATH_SEP_STR = "/";
#endif

#if defined(__clang__)
    constexpr const char* COMPILER_NAME = "Clang";
#elif defined(__GNUC__)
    constexpr const char* COMPILER_NAME = "GCC";
#elif defined(_MSC_VER)
    constexpr const char* COMPILER_NAME = "MSVC";
#else
    constexpr const char* COMPILER_NAME = "Unknown";
#endif

// ============================================================================
// System Limits
// ============================================================================

constexpr size_t MAX_VOLSER_LENGTH = 6;
constexpr size_t MAX_DATASET_NAME_LENGTH = 44;
constexpr size_t MAX_OWNER_LENGTH = 8;
constexpr size_t MAX_POOL_NAME_LENGTH = 8;
constexpr size_t MAX_TAG_LENGTH = 32;
constexpr size_t MAX_TAGS_PER_VOLUME = 100;
constexpr size_t MAX_AUDIT_ENTRIES = 10000;
constexpr size_t MAX_RESERVATION_HOURS = 168;  // 1 week

// v3.2.0 limits
constexpr size_t MAX_SNAPSHOT_HISTORY = 100;
constexpr size_t MAX_LOCATION_HISTORY = 50;
constexpr size_t MAX_PARALLEL_OPERATIONS = 16;
constexpr int DEFAULT_HEALTH_CHECK_INTERVAL_SECONDS = 3600;
constexpr size_t FUZZY_SEARCH_THRESHOLD = 2;

// v3.3.0 limits
constexpr size_t MAX_ENCRYPTION_KEY_ID_LENGTH = 64;
constexpr size_t MAX_PROFILE_NAME_LENGTH = 32;
constexpr size_t MAX_PROFILES = 50;
constexpr size_t MAX_RETRY_ATTEMPTS = 5;
constexpr int DEFAULT_RETRY_DELAY_MS = 100;
constexpr double DEFAULT_BACKOFF_MULTIPLIER = 2.0;

// ============================================================================
// Feature Flags
// ============================================================================

constexpr bool FEATURE_TAGGING = true;
constexpr bool FEATURE_RESERVATIONS = true;
constexpr bool FEATURE_HEALTH_CHECKS = true;
constexpr bool FEATURE_BATCH_OPERATIONS = true;
constexpr bool FEATURE_CSV_EXPORT = true;
constexpr bool FEATURE_SECONDARY_INDICES = true;
constexpr bool FEATURE_REGEX_CACHE = true;
constexpr bool FEATURE_AUDIT_PRUNING = true;

// v3.0.0 features
constexpr bool FEATURE_JSON = true;
constexpr bool FEATURE_GROUPS = true;
constexpr bool FEATURE_RETENTION = true;
constexpr bool FEATURE_REPORTS = true;
constexpr bool FEATURE_BACKUP = true;
constexpr bool FEATURE_EVENTS = true;
constexpr bool FEATURE_HISTORY = true;
constexpr bool FEATURE_INTEGRITY = true;
constexpr bool FEATURE_QUERY = true;

// v3.2.0 features
constexpr bool FEATURE_SNAPSHOTS = true;
constexpr bool FEATURE_VOLUME_HEALTH = true;
constexpr bool FEATURE_FUZZY_SEARCH = true;
constexpr bool FEATURE_AUTO_LIFECYCLE = true;
constexpr bool FEATURE_LOCATION_HISTORY = true;

// v3.3.0 features
constexpr bool FEATURE_ENCRYPTION_METADATA = true;
constexpr bool FEATURE_STORAGE_TIERING = true;
constexpr bool FEATURE_QUOTA_MANAGEMENT = true;
constexpr bool FEATURE_AUDIT_EXPORT = true;
constexpr bool FEATURE_CONFIG_PROFILES = true;
constexpr bool FEATURE_STATS_AGGREGATION = true;
constexpr bool FEATURE_PARALLEL_BATCH = true;
constexpr bool FEATURE_ERROR_RECOVERY = true;

// ============================================================================
// Helper Functions
// ============================================================================

inline std::string get_version_string() {
    return std::string(VERSION_STRING) + " (" + PLATFORM_NAME + "/" + COMPILER_NAME + ")";
}

inline std::string get_version_banner() {
    std::ostringstream oss;
    oss << "TMS Tape Management System v" << VERSION_STRING << "\n"
        << "Catalog Version: " << CATALOG_VERSION << "\n"
        << "Platform: " << PLATFORM_NAME << "\n"
        << "Compiler: " << COMPILER_NAME << "\n"
        << "Build Date: " << VERSION_DATE;
    return oss.str();
}

inline bool is_catalog_compatible(int version) {
    return version >= MIN_CATALOG_VERSION && version <= CATALOG_VERSION;
}

inline int get_version_number() {
    return VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;
}

inline std::vector<std::string> get_enabled_features() {
    std::vector<std::string> features;
    if (FEATURE_TAGGING) features.push_back("Tagging");
    if (FEATURE_RESERVATIONS) features.push_back("Reservations");
    if (FEATURE_HEALTH_CHECKS) features.push_back("Health Checks");
    if (FEATURE_BATCH_OPERATIONS) features.push_back("Batch Operations");
    if (FEATURE_CSV_EXPORT) features.push_back("CSV Export");
    if (FEATURE_SECONDARY_INDICES) features.push_back("Secondary Indices");
    if (FEATURE_REGEX_CACHE) features.push_back("Regex Cache");
    if (FEATURE_AUDIT_PRUNING) features.push_back("Audit Pruning");
    if (FEATURE_JSON) features.push_back("JSON Support");
    if (FEATURE_GROUPS) features.push_back("Volume Groups");
    if (FEATURE_RETENTION) features.push_back("Retention Policies");
    if (FEATURE_REPORTS) features.push_back("Report Generation");
    if (FEATURE_BACKUP) features.push_back("Backup Management");
    if (FEATURE_EVENTS) features.push_back("Event System");
    if (FEATURE_HISTORY) features.push_back("Statistics History");
    if (FEATURE_INTEGRITY) features.push_back("Integrity Checking");
    if (FEATURE_QUERY) features.push_back("Query Engine");
    if (FEATURE_SNAPSHOTS) features.push_back("Volume Snapshots");
    if (FEATURE_VOLUME_HEALTH) features.push_back("Volume Health");
    if (FEATURE_FUZZY_SEARCH) features.push_back("Fuzzy Search");
    if (FEATURE_AUTO_LIFECYCLE) features.push_back("Auto Lifecycle");
    if (FEATURE_LOCATION_HISTORY) features.push_back("Location History");
    if (FEATURE_ENCRYPTION_METADATA) features.push_back("Encryption Metadata");
    if (FEATURE_STORAGE_TIERING) features.push_back("Storage Tiering");
    if (FEATURE_QUOTA_MANAGEMENT) features.push_back("Quota Management");
    if (FEATURE_AUDIT_EXPORT) features.push_back("Audit Export");
    if (FEATURE_CONFIG_PROFILES) features.push_back("Config Profiles");
    if (FEATURE_STATS_AGGREGATION) features.push_back("Stats Aggregation");
    if (FEATURE_PARALLEL_BATCH) features.push_back("Parallel Batch");
    if (FEATURE_ERROR_RECOVERY) features.push_back("Error Recovery");
    return features;
}

} // namespace tms

#endif // TMS_VERSION_H
