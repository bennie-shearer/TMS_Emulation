/**
 * @file tms_utils.h
 * @brief TMS Tape Management System - Utility Functions
 * @version 3.3.0
 * @date 2026-01-08
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_UTILS_H
#define TMS_UTILS_H

#include "tms_types.h"
#include "tms_version.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <regex>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace tms {

// ============================================================================
// Regex Pattern Cache (Thread-Safe)
// ============================================================================

/**
 * @brief Thread-safe regex pattern cache for improved performance
 */
class RegexCache {
public:
    static RegexCache& instance() {
        static RegexCache instance;
        return instance;
    }
    
    const std::regex& get(const std::string& pattern, 
                          std::regex_constants::syntax_option_type flags = std::regex::icase) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string key = pattern + ":" + std::to_string(static_cast<int>(flags));
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        
        auto [inserted, success] = cache_.emplace(key, std::regex(pattern, flags));
        
        if (cache_.size() > MAX_CACHE_SIZE) {
            prune_cache();
        }
        
        return inserted->second;
    }
    
    bool has(const std::string& pattern) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.find(pattern + ":" + std::to_string(static_cast<int>(std::regex::icase))) != cache_.end();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
private:
    RegexCache() = default;
    RegexCache(const RegexCache&) = delete;
    RegexCache& operator=(const RegexCache&) = delete;
    
    void prune_cache() {
        size_t to_remove = cache_.size() / 2;
        auto it = cache_.begin();
        while (to_remove-- > 0 && it != cache_.end()) {
            it = cache_.erase(it);
        }
    }
    
    static constexpr size_t MAX_CACHE_SIZE = 1000;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::regex> cache_;
};

// ============================================================================
// Time Formatting Functions
// ============================================================================

inline std::string format_time(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

inline std::chrono::system_clock::time_point parse_time(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

inline std::string format_duration(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = duration;
    
    std::ostringstream oss;
    if (hours.count() > 0) {
        oss << hours.count() << "h ";
    }
    if (minutes.count() > 0 || hours.count() > 0) {
        oss << minutes.count() << "m ";
    }
    oss << seconds.count() << "s";
    return oss.str();
}

inline std::string get_timestamp() {
    return format_time(std::chrono::system_clock::now());
}

// ============================================================================
// Byte Formatting
// ============================================================================

inline std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
        unit++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
}

// ============================================================================
// Status Conversion Functions
// ============================================================================

inline std::string volume_status_to_string(VolumeStatus status) {
    switch (status) {
        case VolumeStatus::SCRATCH: return "SCRATCH";
        case VolumeStatus::PRIVATE: return "PRIVATE";
        case VolumeStatus::ARCHIVED: return "ARCHIVED";
        case VolumeStatus::EXPIRED: return "EXPIRED";
        case VolumeStatus::MOUNTED: return "MOUNTED";
        case VolumeStatus::OFFLINE: return "OFFLINE";
        case VolumeStatus::RESERVED: return "RESERVED";
        case VolumeStatus::VOLUME_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

inline VolumeStatus string_to_volume_status(const std::string& str) {
    if (str == "SCRATCH") return VolumeStatus::SCRATCH;
    if (str == "PRIVATE") return VolumeStatus::PRIVATE;
    if (str == "ARCHIVED") return VolumeStatus::ARCHIVED;
    if (str == "EXPIRED") return VolumeStatus::EXPIRED;
    if (str == "MOUNTED") return VolumeStatus::MOUNTED;
    if (str == "OFFLINE") return VolumeStatus::OFFLINE;
    if (str == "RESERVED") return VolumeStatus::RESERVED;
    if (str == "ERROR") return VolumeStatus::VOLUME_ERROR;
    return VolumeStatus::SCRATCH;
}

inline std::string dataset_status_to_string(DatasetStatus status) {
    switch (status) {
        case DatasetStatus::ACTIVE: return "ACTIVE";
        case DatasetStatus::MIGRATED: return "MIGRATED";
        case DatasetStatus::EXPIRED: return "EXPIRED";
        case DatasetStatus::DELETED: return "DELETED";
        case DatasetStatus::RECALLED: return "RECALLED";
        case DatasetStatus::PENDING: return "PENDING";
        default: return "UNKNOWN";
    }
}

inline DatasetStatus string_to_dataset_status(const std::string& str) {
    if (str == "ACTIVE") return DatasetStatus::ACTIVE;
    if (str == "MIGRATED") return DatasetStatus::MIGRATED;
    if (str == "EXPIRED") return DatasetStatus::EXPIRED;
    if (str == "DELETED") return DatasetStatus::DELETED;
    if (str == "RECALLED") return DatasetStatus::RECALLED;
    if (str == "PENDING") return DatasetStatus::PENDING;
    return DatasetStatus::ACTIVE;
}

inline std::string density_to_string(TapeDensity density) {
    switch (density) {
        case TapeDensity::DENSITY_800BPI: return "800BPI";
        case TapeDensity::DENSITY_1600BPI: return "1600BPI";
        case TapeDensity::DENSITY_6250BPI: return "6250BPI";
        case TapeDensity::DENSITY_3480: return "3480";
        case TapeDensity::DENSITY_3490: return "3490";
        case TapeDensity::DENSITY_3590: return "3590";
        case TapeDensity::DENSITY_LTO1: return "LTO-1";
        case TapeDensity::DENSITY_LTO2: return "LTO-2";
        case TapeDensity::DENSITY_LTO3: return "LTO-3";
        case TapeDensity::DENSITY_LTO4: return "LTO-4";
        case TapeDensity::DENSITY_LTO5: return "LTO-5";
        case TapeDensity::DENSITY_LTO6: return "LTO-6";
        case TapeDensity::DENSITY_LTO7: return "LTO-7";
        case TapeDensity::DENSITY_LTO8: return "LTO-8";
        case TapeDensity::DENSITY_LTO9: return "LTO-9";
        default: return "UNKNOWN";
    }
}

inline TapeDensity string_to_density(const std::string& str) {
    if (str == "800BPI") return TapeDensity::DENSITY_800BPI;
    if (str == "1600BPI") return TapeDensity::DENSITY_1600BPI;
    if (str == "6250BPI") return TapeDensity::DENSITY_6250BPI;
    if (str == "3480") return TapeDensity::DENSITY_3480;
    if (str == "3490") return TapeDensity::DENSITY_3490;
    if (str == "3590") return TapeDensity::DENSITY_3590;
    if (str == "LTO-1") return TapeDensity::DENSITY_LTO1;
    if (str == "LTO-2") return TapeDensity::DENSITY_LTO2;
    if (str == "LTO-3") return TapeDensity::DENSITY_LTO3;
    if (str == "LTO-4") return TapeDensity::DENSITY_LTO4;
    if (str == "LTO-5") return TapeDensity::DENSITY_LTO5;
    if (str == "LTO-6") return TapeDensity::DENSITY_LTO6;
    if (str == "LTO-7") return TapeDensity::DENSITY_LTO7;
    if (str == "LTO-8") return TapeDensity::DENSITY_LTO8;
    if (str == "LTO-9") return TapeDensity::DENSITY_LTO9;
    return TapeDensity::DENSITY_LTO3;
}

inline uint64_t get_density_capacity(TapeDensity density) {
    switch (density) {
        case TapeDensity::DENSITY_800BPI: return 40ULL * 1024 * 1024;
        case TapeDensity::DENSITY_1600BPI: return 80ULL * 1024 * 1024;
        case TapeDensity::DENSITY_6250BPI: return 170ULL * 1024 * 1024;
        case TapeDensity::DENSITY_3480: return 200ULL * 1024 * 1024;
        case TapeDensity::DENSITY_3490: return 800ULL * 1024 * 1024;
        case TapeDensity::DENSITY_3590: return 60ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO1: return 100ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO2: return 200ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO3: return 400ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO4: return 800ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO5: return 1500ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO6: return 2500ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO7: return 6000ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO8: return 12000ULL * 1024 * 1024 * 1024;
        case TapeDensity::DENSITY_LTO9: return 18000ULL * 1024 * 1024 * 1024;
        default: return 400ULL * 1024 * 1024 * 1024;
    }
}

// v3.2.0: Health status conversion
inline std::string health_status_to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::EXCELLENT: return "EXCELLENT";
        case HealthStatus::GOOD: return "GOOD";
        case HealthStatus::FAIR: return "FAIR";
        case HealthStatus::POOR: return "POOR";
        case HealthStatus::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

inline HealthStatus string_to_health_status(const std::string& str) {
    if (str == "EXCELLENT") return HealthStatus::EXCELLENT;
    if (str == "GOOD") return HealthStatus::GOOD;
    if (str == "FAIR") return HealthStatus::FAIR;
    if (str == "POOR") return HealthStatus::POOR;
    if (str == "CRITICAL") return HealthStatus::CRITICAL;
    return HealthStatus::GOOD;
}

// v3.2.0: Lifecycle action conversion
inline std::string lifecycle_action_to_string(LifecycleAction action) {
    switch (action) {
        case LifecycleAction::NONE: return "NONE";
        case LifecycleAction::WARN: return "WARN";
        case LifecycleAction::MIGRATE: return "MIGRATE";
        case LifecycleAction::ARCHIVE: return "ARCHIVE";
        case LifecycleAction::SCRATCH: return "SCRATCH";
        case LifecycleAction::RETIRE: return "RETIRE";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Validation Functions
// ============================================================================

inline bool validate_volser(const std::string& volser) {
    if (volser.empty() || volser.length() > MAX_VOLSER_LENGTH) return false;
    for (char c : volser) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

inline bool validate_dataset_name(const std::string& name) {
    if (name.empty() || name.length() > MAX_DATASET_NAME_LENGTH) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && 
            c != '.' && c != '-' && c != '_') return false;
    }
    return true;
}

inline bool validate_tag(const std::string& tag) {
    if (tag.empty() || tag.length() > MAX_TAG_LENGTH) return false;
    for (char c : tag) {
        if (std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

inline bool validate_owner(const std::string& owner) {
    if (owner.empty() || owner.length() > MAX_OWNER_LENGTH) return false;
    for (char c : owner) {
        if (!std::isalnum(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// ============================================================================
// String Utility Functions
// ============================================================================

inline std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return str;
}

inline std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// ============================================================================
// v3.2.0: Fuzzy Matching (Levenshtein Distance)
// ============================================================================

/**
 * @brief Calculate Levenshtein edit distance between two strings
 */
inline size_t levenshtein_distance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size();
    const size_t n = s2.size();
    
    if (m == 0) return n;
    if (n == 0) return m;
    
    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));
    
    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;
    
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (std::tolower(s1[i-1]) == std::tolower(s2[j-1])) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,      // deletion
                dp[i][j-1] + 1,      // insertion
                dp[i-1][j-1] + cost  // substitution
            });
        }
    }
    
    return dp[m][n];
}

/**
 * @brief Check if two strings are similar within a threshold
 */
inline bool fuzzy_match(const std::string& text, const std::string& pattern, size_t threshold = 2) {
    // Quick checks
    if (pattern.empty()) return true;
    if (text.empty()) return pattern.empty();
    
    // If length difference exceeds threshold, can't match
    size_t len_diff = (text.length() > pattern.length()) ? 
                      (text.length() - pattern.length()) : 
                      (pattern.length() - text.length());
    if (len_diff > threshold) return false;
    
    return levenshtein_distance(text, pattern) <= threshold;
}

/**
 * @brief Get similarity score (0.0 to 1.0) between two strings
 */
inline double similarity_score(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 1.0;
    if (s1.empty() || s2.empty()) return 0.0;
    
    size_t distance = levenshtein_distance(s1, s2);
    size_t max_len = std::max(s1.length(), s2.length());
    
    return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

// ============================================================================
// Pattern Matching (with Regex Cache)
// ============================================================================

inline std::string wildcard_to_regex(const std::string& pattern) {
    std::string regex_pattern;
    for (char c : pattern) {
        if (c == '*') regex_pattern += ".*";
        else if (c == '?') regex_pattern += ".";
        else if (std::string("[](){}^$.|\\+").find(c) != std::string::npos) {
            regex_pattern += '\\';
            regex_pattern += c;
        } else {
            regex_pattern += c;
        }
    }
    return regex_pattern;
}

inline bool matches_pattern(const std::string& text, const std::string& pattern, 
                           SearchMode mode, size_t fuzzy_threshold = 2) {
    switch (mode) {
        case SearchMode::EXACT:
            return text == pattern;
            
        case SearchMode::PREFIX:
            return text.length() >= pattern.length() &&
                   text.substr(0, pattern.length()) == pattern;
            
        case SearchMode::SUFFIX:
            return text.length() >= pattern.length() &&
                   text.substr(text.length() - pattern.length()) == pattern;
            
        case SearchMode::CONTAINS:
            return text.find(pattern) != std::string::npos;
            
        case SearchMode::WILDCARD: {
            try {
                std::string regex_pattern = wildcard_to_regex(pattern);
                const std::regex& re = RegexCache::instance().get(regex_pattern);
                return std::regex_match(text, re);
            } catch (...) {
                return false;
            }
        }
        
        case SearchMode::REGEX: {
            try {
                const std::regex& re = RegexCache::instance().get(pattern);
                return std::regex_match(text, re);
            } catch (...) {
                return false;
            }
        }
        
        case SearchMode::FUZZY:
            return fuzzy_match(text, pattern, fuzzy_threshold);
        
        default:
            return false;
    }
}

// ============================================================================
// v3.2.0: Volume Health Calculation
// ============================================================================

/**
 * @brief Calculate health score for a volume
 */
inline VolumeHealthScore calculate_health_score(const TapeVolume& vol) {
    VolumeHealthScore score;
    score.last_calculated = std::chrono::system_clock::now();
    
    // Error rate score (0-100)
    int total_errors = vol.get_total_errors();
    if (total_errors == 0) {
        score.error_rate_score = 100.0;
    } else if (total_errors < 5) {
        score.error_rate_score = 80.0;
    } else if (total_errors < 10) {
        score.error_rate_score = 60.0;
    } else if (total_errors < 20) {
        score.error_rate_score = 40.0;
    } else {
        score.error_rate_score = std::max(0.0, 100.0 - total_errors * 2.0);
    }
    
    // Age score (based on typical tape lifetime of 15-30 years)
    int age_days = vol.get_age_days();
    int age_years = age_days / 365;
    if (age_years < 5) {
        score.age_score = 100.0;
    } else if (age_years < 10) {
        score.age_score = 90.0;
    } else if (age_years < 15) {
        score.age_score = 70.0;
    } else if (age_years < 20) {
        score.age_score = 50.0;
    } else {
        score.age_score = std::max(10.0, 50.0 - (age_years - 20) * 5.0);
    }
    
    // Usage score (based on mount count)
    if (vol.mount_count < 100) {
        score.usage_score = 100.0;
    } else if (vol.mount_count < 500) {
        score.usage_score = 90.0;
    } else if (vol.mount_count < 1000) {
        score.usage_score = 70.0;
    } else if (vol.mount_count < 5000) {
        score.usage_score = 50.0;
    } else {
        score.usage_score = std::max(10.0, 100.0 - vol.mount_count / 100.0);
    }
    
    // Capacity score
    double usage_pct = vol.get_usage_percent();
    if (usage_pct < 80.0) {
        score.capacity_score = 100.0;
    } else if (usage_pct < 90.0) {
        score.capacity_score = 80.0;
    } else if (usage_pct < 95.0) {
        score.capacity_score = 60.0;
    } else {
        score.capacity_score = 40.0;
    }
    
    // Calculate overall score (weighted average)
    score.overall_score = (score.error_rate_score * 0.35 +
                          score.age_score * 0.25 +
                          score.usage_score * 0.25 +
                          score.capacity_score * 0.15);
    
    score.status = VolumeHealthScore::score_to_status(score.overall_score);
    
    // Generate recommendations
    if (score.error_rate_score < 60.0) {
        score.recommendations.push_back("High error rate - consider replacing volume");
    }
    if (score.age_score < 50.0) {
        score.recommendations.push_back("Volume aging - plan for replacement");
    }
    if (score.usage_score < 50.0) {
        score.recommendations.push_back("High mount count - monitor for wear");
    }
    if (score.capacity_score < 60.0) {
        score.recommendations.push_back("Near capacity - consider data migration");
    }
    
    return score;
}

// ============================================================================
// v3.2.0: Snapshot ID Generation
// ============================================================================

/**
 * @brief Generate unique snapshot ID
 */
inline std::string generate_snapshot_id(const std::string& volser) {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << "SNAP-" << volser << "-" 
        << std::put_time(std::localtime(&time_t_val), "%Y%m%d%H%M%S")
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// ============================================================================
// Dataset Implementation
// ============================================================================

inline std::string Dataset::get_full_name() const {
    if (is_gdg()) {
        std::ostringstream oss;
        oss << name << ".G" << std::setw(4) << std::setfill('0') << generation
            << "V" << std::setw(2) << std::setfill('0') << version;
        return oss.str();
    }
    return name;
}

} // namespace tms

#endif // TMS_UTILS_H
