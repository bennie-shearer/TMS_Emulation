/**
 * @file tms_types.h
 * @brief TMS Tape Management System - Type Definitions
 * @version 3.3.0
 * @date 2026-01-08
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_TYPES_H
#define TMS_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <chrono>
#include <deque>
#include <algorithm>
#include <cmath>

namespace tms {

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Volume status values
 */
enum class VolumeStatus {
    SCRATCH,        ///< Available for allocation
    PRIVATE,        ///< Contains data
    ARCHIVED,       ///< Archived to vault
    EXPIRED,        ///< Past expiration date
    MOUNTED,        ///< Currently mounted
    OFFLINE,        ///< Taken offline
    RESERVED,       ///< Reserved for exclusive use
    VOLUME_ERROR    ///< Error condition
};

/**
 * @brief Dataset status values
 */
enum class DatasetStatus {
    ACTIVE,         ///< In use
    MIGRATED,       ///< Migrated to ML2
    EXPIRED,        ///< Past expiration
    DELETED,        ///< Marked for deletion
    RECALLED,       ///< Recalled from migration
    PENDING         ///< Pending operation
};

/**
 * @brief Tape density/technology types
 */
enum class TapeDensity {
    DENSITY_800BPI,
    DENSITY_1600BPI,
    DENSITY_6250BPI,
    DENSITY_3480,
    DENSITY_3490,
    DENSITY_3590,
    DENSITY_LTO1,
    DENSITY_LTO2,
    DENSITY_LTO3,
    DENSITY_LTO4,
    DENSITY_LTO5,
    DENSITY_LTO6,
    DENSITY_LTO7,
    DENSITY_LTO8,
    DENSITY_LTO9
};

/**
 * @brief Search pattern matching modes
 */
enum class SearchMode {
    EXACT,          ///< Exact match
    PREFIX,         ///< Starts with pattern
    SUFFIX,         ///< Ends with pattern
    CONTAINS,       ///< Contains pattern
    WILDCARD,       ///< Wildcard (* and ?)
    REGEX,          ///< Regular expression
    FUZZY           ///< v3.2.0: Fuzzy matching (edit distance)
};

/**
 * @brief v3.2.0: Volume health status
 */
enum class HealthStatus {
    EXCELLENT,      ///< Health score >= 90%
    GOOD,           ///< Health score >= 70%
    FAIR,           ///< Health score >= 50%
    POOR,           ///< Health score >= 30%
    CRITICAL        ///< Health score < 30%
};

/**
 * @brief v3.2.0: Lifecycle action types
 */
enum class LifecycleAction {
    NONE,           ///< No action required
    WARN,           ///< Warning threshold reached
    MIGRATE,        ///< Should migrate data
    ARCHIVE,        ///< Should archive
    SCRATCH,        ///< Should return to scratch
    RETIRE          ///< Should retire volume
};

// ============================================================================
// v3.2.0: New Data Structures
// ============================================================================

/**
 * @brief v3.2.0: Location history entry for tracking volume movements
 */
struct LocationHistoryEntry {
    std::string location;                                   ///< Location value
    std::chrono::system_clock::time_point timestamp;        ///< When moved
    std::string moved_by;                                   ///< User who moved it
    std::string reason;                                     ///< Reason for move
};

/**
 * @brief v3.2.0: Volume health score with component metrics
 */
struct VolumeHealthScore {
    double overall_score = 100.0;                           ///< Overall health (0-100)
    HealthStatus status = HealthStatus::EXCELLENT;          ///< Categorized status
    double error_rate_score = 100.0;                        ///< Based on error count
    double age_score = 100.0;                               ///< Based on volume age
    double usage_score = 100.0;                             ///< Based on mount count
    double capacity_score = 100.0;                          ///< Based on capacity usage
    std::chrono::system_clock::time_point last_calculated;  ///< Last calculation time
    std::vector<std::string> recommendations;               ///< Health recommendations
    
    /// Calculate status from score
    static HealthStatus score_to_status(double score) {
        if (score >= 90.0) return HealthStatus::EXCELLENT;
        if (score >= 70.0) return HealthStatus::GOOD;
        if (score >= 50.0) return HealthStatus::FAIR;
        if (score >= 30.0) return HealthStatus::POOR;
        return HealthStatus::CRITICAL;
    }
    
    /// Check if health is acceptable
    bool is_healthy() const {
        return status != HealthStatus::POOR && status != HealthStatus::CRITICAL;
    }
};

/**
 * @brief v3.2.0: Volume snapshot for point-in-time state capture
 */
struct VolumeSnapshot {
    std::string snapshot_id;                                ///< Unique snapshot ID
    std::string volser;                                     ///< Source volume
    std::chrono::system_clock::time_point created;          ///< Creation time
    std::string created_by;                                 ///< Creator
    std::string description;                                ///< Snapshot description
    VolumeStatus status_at_snapshot;                        ///< Status when snapshotted
    std::vector<std::string> datasets_at_snapshot;          ///< Datasets present
    uint64_t used_bytes_at_snapshot = 0;                    ///< Used space
    int mount_count_at_snapshot = 0;                        ///< Mount count
    std::set<std::string> tags_at_snapshot;                 ///< Tags present
    std::string notes_at_snapshot;                          ///< Notes
};

/**
 * @brief v3.2.0: Lifecycle recommendation
 */
struct LifecycleRecommendation {
    std::string volser;                                     ///< Target volume
    LifecycleAction action;                                 ///< Recommended action
    std::string reason;                                     ///< Why this action
    int priority = 0;                                       ///< Priority (1-10, 10=urgent)
    std::chrono::system_clock::time_point due_date;         ///< When action should occur
    bool auto_actionable = false;                           ///< Can be auto-executed
};

// ============================================================================
// v3.3.0: New Data Structures
// ============================================================================

/**
 * @brief v3.3.0: Encryption algorithm types
 */
enum class EncryptionAlgorithm {
    NONE,           ///< No encryption
    AES_128,        ///< AES 128-bit
    AES_256,        ///< AES 256-bit
    TDES            ///< Triple DES
};

/**
 * @brief v3.3.0: Encryption metadata for volumes
 */
struct EncryptionMetadata {
    bool encrypted = false;
    EncryptionAlgorithm algorithm = EncryptionAlgorithm::NONE;
    std::string key_id;
    std::string key_label;
    std::chrono::system_clock::time_point encrypted_date;
    std::string encrypted_by;
    
    bool is_encrypted() const { return encrypted && algorithm != EncryptionAlgorithm::NONE; }
};

/**
 * @brief v3.3.0: Storage tier types
 */
enum class StorageTier {
    HOT,            ///< Frequently accessed
    WARM,           ///< Occasionally accessed
    COLD,           ///< Rarely accessed
    ARCHIVE         ///< Long-term retention
};

/**
 * @brief v3.3.0: Tier policy for automatic tiering
 */
struct TierPolicy {
    StorageTier tier;
    int days_inactive_threshold;    ///< Days without access before tier change
    bool auto_migrate = false;
    std::string target_pool;
};

/**
 * @brief v3.3.0: Quota definition
 */
struct Quota {
    std::string name;               ///< Quota identifier (pool or owner)
    uint64_t max_bytes = 0;         ///< Maximum capacity in bytes (0 = unlimited)
    uint64_t max_volumes = 0;       ///< Maximum number of volumes (0 = unlimited)
    uint64_t used_bytes = 0;        ///< Currently used bytes
    uint64_t used_volumes = 0;      ///< Currently used volumes
    bool enabled = true;
    
    double get_bytes_usage_percent() const {
        return max_bytes > 0 ? (static_cast<double>(used_bytes) / max_bytes) * 100.0 : 0.0;
    }
    
    double get_volumes_usage_percent() const {
        return max_volumes > 0 ? (static_cast<double>(used_volumes) / max_volumes) * 100.0 : 0.0;
    }
    
    bool is_bytes_exceeded() const { return max_bytes > 0 && used_bytes > max_bytes; }
    bool is_volumes_exceeded() const { return max_volumes > 0 && used_volumes > max_volumes; }
};

/**
 * @brief v3.3.0: Audit export format
 */
enum class AuditExportFormat {
    TEXT,           ///< Plain text
    CSV,            ///< Comma-separated values
    JSON            ///< JSON format
};

/**
 * @brief v3.3.0: Configuration profile
 */
struct ConfigProfile {
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point created;
    std::string created_by;
    std::map<std::string, std::string> settings;
    
    std::string get_setting(const std::string& key, const std::string& default_val = "") const {
        auto it = settings.find(key);
        return it != settings.end() ? it->second : default_val;
    }
};

/**
 * @brief v3.3.0: Statistical aggregation results
 */
struct StatisticsAggregation {
    double min_value = 0.0;
    double max_value = 0.0;
    double avg_value = 0.0;
    double sum_value = 0.0;
    double median_value = 0.0;
    double std_deviation = 0.0;
    double percentile_25 = 0.0;
    double percentile_75 = 0.0;
    double percentile_90 = 0.0;
    double percentile_95 = 0.0;
    size_t count = 0;
};

/**
 * @brief v3.3.0: Retry policy for error recovery
 */
struct RetryPolicy {
    size_t max_attempts = 3;
    int initial_delay_ms = 100;
    double backoff_multiplier = 2.0;
    int max_delay_ms = 5000;
    bool retry_on_timeout = true;
    bool retry_on_io_error = true;
};

/**
 * @brief v3.3.0: Operation result with retry information
 */
struct RetryableResult {
    bool success = false;
    int attempts_made = 0;
    int total_delay_ms = 0;
    std::string last_error;
    std::vector<std::string> attempt_errors;
    
    bool required_retry() const { return attempts_made > 1; }
};

// Helper functions for v3.3.0 types
inline std::string encryption_algorithm_to_string(EncryptionAlgorithm algo) {
    switch (algo) {
        case EncryptionAlgorithm::NONE: return "NONE";
        case EncryptionAlgorithm::AES_128: return "AES-128";
        case EncryptionAlgorithm::AES_256: return "AES-256";
        case EncryptionAlgorithm::TDES: return "3DES";
        default: return "UNKNOWN";
    }
}

inline EncryptionAlgorithm string_to_encryption_algorithm(const std::string& str) {
    if (str == "AES-128" || str == "AES128") return EncryptionAlgorithm::AES_128;
    if (str == "AES-256" || str == "AES256") return EncryptionAlgorithm::AES_256;
    if (str == "3DES" || str == "TDES") return EncryptionAlgorithm::TDES;
    return EncryptionAlgorithm::NONE;
}

inline std::string storage_tier_to_string(StorageTier tier) {
    switch (tier) {
        case StorageTier::HOT: return "HOT";
        case StorageTier::WARM: return "WARM";
        case StorageTier::COLD: return "COLD";
        case StorageTier::ARCHIVE: return "ARCHIVE";
        default: return "UNKNOWN";
    }
}

inline StorageTier string_to_storage_tier(const std::string& str) {
    if (str == "HOT") return StorageTier::HOT;
    if (str == "WARM") return StorageTier::WARM;
    if (str == "COLD") return StorageTier::COLD;
    if (str == "ARCHIVE") return StorageTier::ARCHIVE;
    return StorageTier::HOT;
}

inline std::string audit_export_format_to_string(AuditExportFormat fmt) {
    switch (fmt) {
        case AuditExportFormat::TEXT: return "TEXT";
        case AuditExportFormat::CSV: return "CSV";
        case AuditExportFormat::JSON: return "JSON";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// v3.3.0: Statistics and Retry Helper Functions
// ============================================================================

inline StatisticsAggregation calculate_statistics(const std::vector<double>& values) {
    StatisticsAggregation stats;
    
    if (values.empty()) {
        return stats;
    }
    
    stats.count = values.size();
    
    // Sort for percentile calculations
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    
    // Min, max, sum
    stats.min_value = sorted.front();
    stats.max_value = sorted.back();
    stats.sum_value = 0.0;
    for (double v : values) {
        stats.sum_value += v;
    }
    
    // Average
    stats.avg_value = stats.sum_value / static_cast<double>(stats.count);
    
    // Median
    size_t mid = stats.count / 2;
    if (stats.count % 2 == 0) {
        stats.median_value = (sorted[mid - 1] + sorted[mid]) / 2.0;
    } else {
        stats.median_value = sorted[mid];
    }
    
    // Standard deviation
    double variance = 0.0;
    for (double v : values) {
        variance += (v - stats.avg_value) * (v - stats.avg_value);
    }
    stats.std_deviation = std::sqrt(variance / static_cast<double>(stats.count));
    
    // Percentiles helper
    auto percentile = [&sorted](double p) -> double {
        if (sorted.empty()) return 0.0;
        double index = (p / 100.0) * static_cast<double>(sorted.size() - 1);
        size_t lower = static_cast<size_t>(index);
        size_t upper = lower + 1;
        if (upper >= sorted.size()) return sorted.back();
        double weight = index - static_cast<double>(lower);
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    };
    
    stats.percentile_25 = percentile(25.0);
    stats.percentile_75 = percentile(75.0);
    stats.percentile_90 = percentile(90.0);
    stats.percentile_95 = percentile(95.0);
    
    return stats;
}

inline int calculate_retry_delay(const RetryPolicy& policy, int attempt) {
    int delay = policy.initial_delay_ms;
    for (int i = 1; i < attempt; ++i) {
        delay = static_cast<int>(static_cast<double>(delay) * policy.backoff_multiplier);
        if (delay > policy.max_delay_ms) {
            delay = policy.max_delay_ms;
            break;
        }
    }
    return delay;
}

// ============================================================================
// Core Data Structures
// ============================================================================

/**
 * @brief Tape volume information
 */
struct TapeVolume {
    std::string volser;                                         ///< Volume serial (1-6 chars)
    VolumeStatus status = VolumeStatus::SCRATCH;                ///< Current status
    TapeDensity density = TapeDensity::DENSITY_LTO3;           ///< Tape density
    std::string location;                                       ///< Physical location
    std::string pool;                                           ///< Pool assignment
    std::string owner;                                          ///< Owner ID
    std::chrono::system_clock::time_point creation_date;        ///< Creation timestamp
    std::chrono::system_clock::time_point expiration_date;      ///< Expiration timestamp
    std::chrono::system_clock::time_point last_used;            ///< Last used timestamp
    int mount_count = 0;                                        ///< Number of mounts
    bool write_protected = false;                               ///< Write protection flag
    uint64_t capacity_bytes = 0;                                ///< Total capacity
    uint64_t used_bytes = 0;                                    ///< Used space
    int error_count = 0;                                        ///< Error counter
    std::vector<std::string> datasets;                          ///< Dataset names on volume
    std::set<std::string> tags;                                 ///< Custom tags
    std::string notes;                                          ///< Free-form notes
    std::string reserved_by;                                    ///< Reservation user
    std::chrono::system_clock::time_point reservation_expires;  ///< Reservation expiry
    
    // v3.2.0: New fields
    std::deque<LocationHistoryEntry> location_history;          ///< Location tracking
    VolumeHealthScore health_score;                             ///< Health metrics
    
    // v3.3.0 fields
    EncryptionMetadata encryption;                              ///< Encryption metadata
    StorageTier storage_tier = StorageTier::HOT;                ///< Storage tier
    std::chrono::system_clock::time_point last_access_date;     ///< Last access time
    std::chrono::system_clock::time_point last_health_check;    ///< Last health check
    std::string media_type;                                     ///< Media type identifier
    int read_error_count = 0;                                   ///< Read errors
    int write_error_count = 0;                                  ///< Write errors
    
    /// Get usage percentage
    double get_usage_percent() const {
        return capacity_bytes > 0 ? 
            (100.0 * static_cast<double>(used_bytes) / static_cast<double>(capacity_bytes)) : 0.0;
    }
    
    /// Get free bytes
    uint64_t get_free_bytes() const {
        return capacity_bytes > used_bytes ? capacity_bytes - used_bytes : 0;
    }
    
    /// Check if volume is expired
    bool is_expired() const {
        return expiration_date < std::chrono::system_clock::now();
    }
    
    /// Check if volume is currently reserved
    bool is_reserved() const {
        return !reserved_by.empty() && reservation_expires > std::chrono::system_clock::now();
    }
    
    /// Check if volume has a specific tag
    bool has_tag(const std::string& tag) const {
        return tags.find(tag) != tags.end();
    }
    
    /// Check if volume is available for scratch allocation
    bool is_available_for_scratch() const {
        return status == VolumeStatus::SCRATCH && !is_reserved() && !is_expired();
    }
    
    /// v3.2.0: Check if volume health is acceptable
    bool is_healthy() const {
        return health_score.is_healthy();
    }
    
    /// v3.2.0: Get total error count
    int get_total_errors() const {
        return error_count + read_error_count + write_error_count;
    }
    
    /// v3.2.0: Get volume age in days
    int get_age_days() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(now - creation_date);
        return static_cast<int>(duration.count() / 24);
    }
};

/**
 * @brief Dataset information
 */
struct Dataset {
    std::string name;                                           ///< Dataset name (1-44 chars)
    std::string volser;                                         ///< Volume reference
    DatasetStatus status = DatasetStatus::ACTIVE;               ///< Current status
    size_t size_bytes = 0;                                      ///< Dataset size
    std::string owner;                                          ///< Owner ID
    std::string job_name;                                       ///< Creating job
    int file_sequence = 1;                                      ///< File sequence number
    int generation = 0;                                         ///< GDG generation
    int version = 0;                                            ///< GDG version
    std::string record_format;                                  ///< RECFM
    size_t block_size = 0;                                      ///< BLKSIZE
    size_t record_length = 0;                                   ///< LRECL
    std::chrono::system_clock::time_point creation_date;        ///< Creation timestamp
    std::chrono::system_clock::time_point expiration_date;      ///< Expiration timestamp
    std::chrono::system_clock::time_point last_accessed;        ///< Last access timestamp
    std::set<std::string> tags;                                 ///< Custom tags
    std::string notes;                                          ///< Free-form notes
    
    // v3.2.0: New fields
    bool compressed = false;                                    ///< Compression flag
    std::string compression_type;                               ///< Compression algorithm
    size_t original_size_bytes = 0;                             ///< Uncompressed size
    int access_count = 0;                                       ///< Access counter
    
    /// Check if this is a GDG dataset
    bool is_gdg() const { return generation > 0; }
    
    /// Check if dataset is expired
    bool is_expired() const { 
        return expiration_date < std::chrono::system_clock::now(); 
    }
    
    /// Check if dataset has a specific tag
    bool has_tag(const std::string& tag) const { 
        return tags.find(tag) != tags.end(); 
    }
    
    /// Get full name including GDG suffix
    std::string get_full_name() const;
    
    /// v3.2.0: Get compression ratio
    double get_compression_ratio() const {
        if (!compressed || original_size_bytes == 0) return 1.0;
        return static_cast<double>(original_size_bytes) / static_cast<double>(size_bytes);
    }
};

/**
 * @brief Audit log record
 */
struct AuditRecord {
    std::chrono::system_clock::time_point timestamp;    ///< When operation occurred
    std::string operation;                              ///< Operation type
    std::string user;                                   ///< User who performed it
    std::string target;                                 ///< Target volume/dataset
    std::string details;                                ///< Additional details
    bool success = true;                                ///< Success flag
    std::string source_ip;                              ///< Source IP address
    std::string session_id;                             ///< Session identifier
};

/**
 * @brief System-wide statistics
 */
struct SystemStatistics {
    size_t total_volumes = 0;
    size_t scratch_volumes = 0;
    size_t private_volumes = 0;
    size_t mounted_volumes = 0;
    size_t expired_volumes = 0;
    size_t reserved_volumes = 0;
    size_t total_datasets = 0;
    size_t active_datasets = 0;
    size_t migrated_datasets = 0;
    size_t expired_datasets = 0;
    uint64_t total_capacity = 0;
    uint64_t used_capacity = 0;
    std::map<std::string, size_t> pool_counts;
    std::chrono::steady_clock::time_point uptime_start;
    size_t operations_count = 0;
    
    // v3.2.0: New statistics
    size_t healthy_volumes = 0;
    size_t unhealthy_volumes = 0;
    size_t snapshots_count = 0;
    double average_health_score = 0.0;
    
    /// Get capacity utilization percentage
    double get_utilization() const {
        return total_capacity > 0 ? 
            (100.0 * static_cast<double>(used_capacity) / static_cast<double>(total_capacity)) : 0.0;
    }
    
    /// Get uptime as formatted string
    std::string get_uptime() const;
    
    /// v3.2.0: Get health ratio
    double get_health_ratio() const {
        if (total_volumes == 0) return 1.0;
        return static_cast<double>(healthy_volumes) / static_cast<double>(total_volumes);
    }
};

/**
 * @brief Pool-specific statistics
 */
struct PoolStatistics {
    std::string pool_name;
    size_t total_volumes = 0;
    size_t scratch_volumes = 0;
    size_t private_volumes = 0;
    size_t mounted_volumes = 0;
    size_t reserved_volumes = 0;
    uint64_t total_capacity = 0;
    uint64_t used_capacity = 0;
    
    // v3.2.0: New statistics
    size_t healthy_volumes = 0;
    double average_health_score = 0.0;
    
    /// Get capacity utilization percentage
    double get_utilization() const {
        return total_capacity > 0 ? 
            (100.0 * static_cast<double>(used_capacity) / static_cast<double>(total_capacity)) : 0.0;
    }
    
    /// Get scratch ratio percentage
    double get_scratch_ratio() const {
        return total_volumes > 0 ? 
            (100.0 * static_cast<double>(scratch_volumes) / static_cast<double>(total_volumes)) : 0.0;
    }
};

/**
 * @brief Batch operation result
 */
struct BatchResult {
    size_t total = 0;                                           ///< Total attempted
    size_t succeeded = 0;                                       ///< Successful operations
    size_t failed = 0;                                          ///< Failed operations
    size_t skipped = 0;                                         ///< Skipped operations
    std::vector<std::pair<std::string, std::string>> failures;  ///< Failure details
    std::chrono::milliseconds duration{0};                      ///< Total duration
    
    /// Check if all operations succeeded
    bool all_succeeded() const { return failed == 0 && skipped == 0; }
    
    /// Check if any operations succeeded
    bool any_succeeded() const { return succeeded > 0; }
    
    /// Get success rate percentage
    double success_rate() const { 
        return total > 0 ? (100.0 * static_cast<double>(succeeded) / static_cast<double>(total)) : 0.0; 
    }
};

/**
 * @brief Search criteria for advanced queries
 */
struct SearchCriteria {
    std::string pattern;                                        ///< Search pattern
    SearchMode mode = SearchMode::CONTAINS;                     ///< Pattern match mode
    std::optional<VolumeStatus> status;                         ///< Filter by status
    std::optional<std::string> owner;                           ///< Filter by owner
    std::optional<std::string> pool;                            ///< Filter by pool
    std::optional<std::string> location;                        ///< Filter by location
    std::optional<std::string> tag;                             ///< Filter by tag
    std::optional<std::chrono::system_clock::time_point> created_after;   ///< Created after date
    std::optional<std::chrono::system_clock::time_point> created_before;  ///< Created before date
    size_t limit = 0;                                           ///< Max results (0 = unlimited)
    
    // v3.2.0: New search criteria
    std::optional<HealthStatus> min_health;                     ///< Minimum health status
    std::optional<int> max_errors;                              ///< Maximum error count
    std::optional<int> min_mount_count;                         ///< Minimum mount count
    std::optional<int> max_mount_count;                         ///< Maximum mount count
    size_t fuzzy_threshold = 2;                                 ///< Max edit distance for fuzzy
};

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    bool healthy = true;                            ///< Overall health status
    std::vector<std::string> warnings;              ///< Warning messages
    std::vector<std::string> errors;                ///< Error messages
    std::map<std::string, std::string> metrics;     ///< Health metrics
    
    // v3.2.0: Enhanced health info
    size_t volumes_checked = 0;                     ///< Number of volumes checked
    size_t unhealthy_count = 0;                     ///< Unhealthy volume count
    std::vector<LifecycleRecommendation> recommendations;  ///< Lifecycle actions
};

} // namespace tms

#endif // TMS_TYPES_H
