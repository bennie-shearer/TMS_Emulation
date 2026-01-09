/**
 * @file tms_history.h
 * @brief TMS Tape Management System - Statistics History Tracking
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides historical statistics tracking for trend analysis
 * and capacity planning.
 */

#ifndef TMS_HISTORY_H
#define TMS_HISTORY_H

#include "tms_types.h"
#include "tms_utils.h"
#include "error_codes.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <fstream>
#include <numeric>

namespace tms {

// ============================================================================
// History Data Types
// ============================================================================

/**
 * @brief Statistics snapshot at a point in time
 */
struct StatisticsSnapshot {
    std::chrono::system_clock::time_point timestamp;
    
    // Volume metrics
    size_t total_volumes = 0;
    size_t scratch_volumes = 0;
    size_t private_volumes = 0;
    size_t mounted_volumes = 0;
    size_t expired_volumes = 0;
    
    // Dataset metrics
    size_t total_datasets = 0;
    size_t active_datasets = 0;
    size_t migrated_datasets = 0;
    
    // Capacity metrics
    uint64_t total_capacity = 0;
    uint64_t used_capacity = 0;
    
    // Operations metrics
    size_t mounts_today = 0;
    size_t scratches_today = 0;
    size_t migrations_today = 0;
    
    /// Calculate utilization percentage
    double get_utilization() const {
        return total_capacity > 0 ?
            (100.0 * static_cast<double>(used_capacity) / static_cast<double>(total_capacity)) : 0.0;
    }
    
    /// Calculate scratch ratio
    double get_scratch_ratio() const {
        return total_volumes > 0 ?
            (100.0 * static_cast<double>(scratch_volumes) / static_cast<double>(total_volumes)) : 0.0;
    }
};

/**
 * @brief Trend direction
 */
enum class TrendDirection {
    UP,
    DOWN,
    STABLE,
    UNKNOWN
};

/**
 * @brief Trend analysis result
 */
struct TrendAnalysis {
    std::string metric_name;
    TrendDirection direction = TrendDirection::UNKNOWN;
    double change_percent = 0.0;
    double average_value = 0.0;
    double min_value = 0.0;
    double max_value = 0.0;
    double current_value = 0.0;
    size_t sample_count = 0;
    std::chrono::system_clock::time_point period_start;
    std::chrono::system_clock::time_point period_end;
};

/**
 * @brief Capacity projection
 */
struct CapacityProjection {
    std::chrono::system_clock::time_point projection_date;
    double projected_utilization = 0.0;
    double confidence = 0.0;  // 0.0 - 1.0
    double daily_growth_rate = 0.0;
    int days_until_80_percent = -1;
    int days_until_90_percent = -1;
    int days_until_full = -1;
};

// ============================================================================
// Statistics History Manager
// ============================================================================

/**
 * @brief Manages historical statistics data
 */
class StatisticsHistory {
public:
    using StatsProvider = std::function<SystemStatistics()>;
    
    StatisticsHistory() = default;
    
    /// Configure data directory
    void set_data_directory(const std::string& dir);
    
    /// Record current statistics
    void record_snapshot(const SystemStatistics& stats);
    
    /// Record from provider function
    void record_snapshot(StatsProvider provider);
    
    /// Get snapshots for time range
    std::vector<StatisticsSnapshot> get_snapshots(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;
    
    /// Get snapshots for last N days
    std::vector<StatisticsSnapshot> get_recent_snapshots(int days) const;
    
    /// Get latest snapshot
    std::optional<StatisticsSnapshot> get_latest_snapshot() const;
    
    /// Trend analysis
    TrendAnalysis analyze_volume_trend(int days) const;
    TrendAnalysis analyze_capacity_trend(int days) const;
    TrendAnalysis analyze_scratch_trend(int days) const;
    TrendAnalysis analyze_custom_metric(const std::string& metric, int days,
        std::function<double(const StatisticsSnapshot&)> extractor) const;
    
    /// Capacity projection
    CapacityProjection project_capacity(int days_ahead) const;
    
    /// Summary reports
    std::map<std::string, double> get_daily_averages(int days) const;
    std::map<std::string, double> get_peak_values(int days) const;
    
    /// Persistence
    OperationResult save_history() const;
    OperationResult load_history();
    OperationResult export_to_csv(const std::string& path) const;
    
    /// Maintenance
    size_t cleanup_old_snapshots(int days_to_keep);
    size_t snapshot_count() const;
    void clear_history();
    
    /// Configuration
    void set_max_snapshots(size_t max) { max_snapshots_ = max; }
    void set_auto_save(bool enable) { auto_save_ = enable; }
    
private:
    StatisticsSnapshot stats_to_snapshot(const SystemStatistics& stats) const;
    std::vector<double> extract_metric(const std::vector<StatisticsSnapshot>& snapshots,
        std::function<double(const StatisticsSnapshot&)> extractor) const;
    TrendDirection calculate_trend(const std::vector<double>& values) const;
    double linear_regression_slope(const std::vector<double>& values) const;
    
    mutable std::mutex mutex_;
    std::vector<StatisticsSnapshot> snapshots_;
    std::string data_directory_;
    size_t max_snapshots_ = 365 * 24;  // ~1 year of hourly snapshots
    bool auto_save_ = true;
};

// ============================================================================
// Implementation
// ============================================================================

inline void StatisticsHistory::set_data_directory(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_directory_ = dir;
}

inline void StatisticsHistory::record_snapshot(const SystemStatistics& stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto snapshot = stats_to_snapshot(stats);
    snapshots_.push_back(snapshot);
    
    // Limit size
    while (snapshots_.size() > max_snapshots_) {
        snapshots_.erase(snapshots_.begin());
    }
    
    if (auto_save_ && !data_directory_.empty()) {
        // Auto-save logic would go here
    }
}

inline void StatisticsHistory::record_snapshot(StatsProvider provider) {
    record_snapshot(provider());
}

inline std::vector<StatisticsSnapshot> StatisticsHistory::get_snapshots(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<StatisticsSnapshot> result;
    for (const auto& s : snapshots_) {
        if (s.timestamp >= start && s.timestamp <= end) {
            result.push_back(s);
        }
    }
    return result;
}

inline std::vector<StatisticsSnapshot> StatisticsHistory::get_recent_snapshots(int days) const {
    auto end = std::chrono::system_clock::now();
    auto start = end - std::chrono::hours(24 * days);
    return get_snapshots(start, end);
}

inline std::optional<StatisticsSnapshot> StatisticsHistory::get_latest_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (snapshots_.empty()) {
        return std::nullopt;
    }
    return snapshots_.back();
}

inline TrendAnalysis StatisticsHistory::analyze_volume_trend(int days) const {
    return analyze_custom_metric("total_volumes", days,
        [](const StatisticsSnapshot& s) { return static_cast<double>(s.total_volumes); });
}

inline TrendAnalysis StatisticsHistory::analyze_capacity_trend(int days) const {
    return analyze_custom_metric("utilization", days,
        [](const StatisticsSnapshot& s) { return s.get_utilization(); });
}

inline TrendAnalysis StatisticsHistory::analyze_scratch_trend(int days) const {
    return analyze_custom_metric("scratch_volumes", days,
        [](const StatisticsSnapshot& s) { return static_cast<double>(s.scratch_volumes); });
}

inline TrendAnalysis StatisticsHistory::analyze_custom_metric(
    const std::string& metric, int days,
    std::function<double(const StatisticsSnapshot&)> extractor) const {
    
    TrendAnalysis result;
    result.metric_name = metric;
    
    auto snapshots = get_recent_snapshots(days);
    if (snapshots.empty()) {
        return result;
    }
    
    result.sample_count = snapshots.size();
    result.period_start = snapshots.front().timestamp;
    result.period_end = snapshots.back().timestamp;
    
    auto values = extract_metric(snapshots, extractor);
    if (values.empty()) {
        return result;
    }
    
    result.current_value = values.back();
    result.min_value = *std::min_element(values.begin(), values.end());
    result.max_value = *std::max_element(values.begin(), values.end());
    result.average_value = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    
    if (values.size() >= 2) {
        result.change_percent = ((values.back() - values.front()) / 
            (values.front() != 0 ? values.front() : 1)) * 100.0;
    }
    
    result.direction = calculate_trend(values);
    
    return result;
}

inline CapacityProjection StatisticsHistory::project_capacity(int days_ahead) const {
    CapacityProjection result;
    result.projection_date = std::chrono::system_clock::now() + 
        std::chrono::hours(24 * days_ahead);
    
    auto snapshots = get_recent_snapshots(30);  // Use 30 days of history
    if (snapshots.size() < 2) {
        result.confidence = 0.0;
        return result;
    }
    
    std::vector<double> utilizations;
    for (const auto& s : snapshots) {
        utilizations.push_back(s.get_utilization());
    }
    
    double slope = linear_regression_slope(utilizations);
    result.daily_growth_rate = slope;
    result.projected_utilization = utilizations.back() + (slope * days_ahead);
    
    // Clamp to valid range
    result.projected_utilization = std::max(0.0, std::min(100.0, result.projected_utilization));
    
    // Calculate days until thresholds
    double current = utilizations.back();
    if (slope > 0) {
        if (current < 80) {
            result.days_until_80_percent = static_cast<int>((80 - current) / slope);
        }
        if (current < 90) {
            result.days_until_90_percent = static_cast<int>((90 - current) / slope);
        }
        if (current < 100) {
            result.days_until_full = static_cast<int>((100 - current) / slope);
        }
    }
    
    // Confidence based on data quality
    result.confidence = std::min(1.0, static_cast<double>(snapshots.size()) / 30.0);
    
    return result;
}

inline std::map<std::string, double> StatisticsHistory::get_daily_averages(int days) const {
    std::map<std::string, double> result;
    
    auto snapshots = get_recent_snapshots(days);
    if (snapshots.empty()) {
        return result;
    }
    
    double total_volumes = 0, scratch_volumes = 0, utilization = 0;
    for (const auto& s : snapshots) {
        total_volumes += s.total_volumes;
        scratch_volumes += s.scratch_volumes;
        utilization += s.get_utilization();
    }
    
    size_t n = snapshots.size();
    result["total_volumes"] = total_volumes / n;
    result["scratch_volumes"] = scratch_volumes / n;
    result["utilization"] = utilization / n;
    
    return result;
}

inline std::map<std::string, double> StatisticsHistory::get_peak_values(int days) const {
    std::map<std::string, double> result;
    
    auto snapshots = get_recent_snapshots(days);
    if (snapshots.empty()) {
        return result;
    }
    
    result["max_volumes"] = 0;
    result["max_utilization"] = 0;
    result["min_scratch"] = std::numeric_limits<double>::max();
    
    for (const auto& s : snapshots) {
        result["max_volumes"] = std::max(result["max_volumes"], 
            static_cast<double>(s.total_volumes));
        result["max_utilization"] = std::max(result["max_utilization"], 
            s.get_utilization());
        result["min_scratch"] = std::min(result["min_scratch"], 
            static_cast<double>(s.scratch_volumes));
    }
    
    return result;
}

inline OperationResult StatisticsHistory::export_to_csv(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open file: " + path);
    }
    
    file << "Timestamp,TotalVolumes,ScratchVolumes,PrivateVolumes,MountedVolumes,"
         << "TotalDatasets,ActiveDatasets,TotalCapacity,UsedCapacity,Utilization\n";
    
    for (const auto& s : snapshots_) {
        file << format_time(s.timestamp) << ","
             << s.total_volumes << ","
             << s.scratch_volumes << ","
             << s.private_volumes << ","
             << s.mounted_volumes << ","
             << s.total_datasets << ","
             << s.active_datasets << ","
             << s.total_capacity << ","
             << s.used_capacity << ","
             << std::fixed << std::setprecision(2) << s.get_utilization() << "\n";
    }
    
    return OperationResult::ok();
}

inline size_t StatisticsHistory::cleanup_old_snapshots(int days_to_keep) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days_to_keep);
    size_t original_size = snapshots_.size();
    
    snapshots_.erase(
        std::remove_if(snapshots_.begin(), snapshots_.end(),
            [cutoff](const StatisticsSnapshot& s) { return s.timestamp < cutoff; }),
        snapshots_.end());
    
    return original_size - snapshots_.size();
}

inline size_t StatisticsHistory::snapshot_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshots_.size();
}

inline void StatisticsHistory::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.clear();
}

inline StatisticsSnapshot StatisticsHistory::stats_to_snapshot(const SystemStatistics& stats) const {
    StatisticsSnapshot s;
    s.timestamp = std::chrono::system_clock::now();
    s.total_volumes = stats.total_volumes;
    s.scratch_volumes = stats.scratch_volumes;
    s.private_volumes = stats.private_volumes;
    s.mounted_volumes = stats.mounted_volumes;
    s.expired_volumes = stats.expired_volumes;
    s.total_datasets = stats.total_datasets;
    s.active_datasets = stats.active_datasets;
    s.migrated_datasets = stats.migrated_datasets;
    s.total_capacity = stats.total_capacity;
    s.used_capacity = stats.used_capacity;
    return s;
}

inline std::vector<double> StatisticsHistory::extract_metric(
    const std::vector<StatisticsSnapshot>& snapshots,
    std::function<double(const StatisticsSnapshot&)> extractor) const {
    
    std::vector<double> values;
    values.reserve(snapshots.size());
    for (const auto& s : snapshots) {
        values.push_back(extractor(s));
    }
    return values;
}

inline TrendDirection StatisticsHistory::calculate_trend(const std::vector<double>& values) const {
    if (values.size() < 2) {
        return TrendDirection::UNKNOWN;
    }
    
    double slope = linear_regression_slope(values);
    double threshold = 0.01 * std::abs(values.front() != 0 ? values.front() : 1);
    
    if (slope > threshold) return TrendDirection::UP;
    if (slope < -threshold) return TrendDirection::DOWN;
    return TrendDirection::STABLE;
}

inline double StatisticsHistory::linear_regression_slope(const std::vector<double>& values) const {
    if (values.size() < 2) return 0.0;
    
    double n = static_cast<double>(values.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    
    for (size_t i = 0; i < values.size(); i++) {
        double x = static_cast<double>(i);
        sum_x += x;
        sum_y += values[i];
        sum_xy += x * values[i];
        sum_xx += x * x;
    }
    
    double denom = n * sum_xx - sum_x * sum_x;
    if (std::abs(denom) < 1e-10) return 0.0;
    
    return (n * sum_xy - sum_x * sum_y) / denom;
}

} // namespace tms

#endif // TMS_HISTORY_H
