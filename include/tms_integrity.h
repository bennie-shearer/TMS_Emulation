/**
 * @file tms_integrity.h
 * @brief TMS Tape Management System - Enhanced Integrity Verification
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides comprehensive catalog integrity verification with
 * checksums, cross-reference validation, and repair suggestions.
 */

#ifndef TMS_INTEGRITY_H
#define TMS_INTEGRITY_H

#include "tms_types.h"
#include "tms_utils.h"
#include "error_codes.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <chrono>

namespace tms {

// ============================================================================
// Integrity Types
// ============================================================================

/**
 * @brief Issue severity level
 */
enum class IssueSeverity {
    INFO,           ///< Informational
    WARNING,        ///< Potential issue
    ERROR,          ///< Definite issue
    CRITICAL        ///< Data corruption
};

/**
 * @brief Issue category
 */
enum class IssueCategory {
    ORPHAN_DATASET,         ///< Dataset references non-existent volume
    ORPHAN_REFERENCE,       ///< Volume references non-existent dataset
    CAPACITY_MISMATCH,      ///< Used bytes exceeds capacity
    STATUS_INCONSISTENCY,   ///< Status doesn't match data
    DUPLICATE_ENTRY,        ///< Duplicate volume or dataset
    INVALID_DATA,           ///< Invalid field values
    MISSING_REQUIRED,       ///< Missing required field
    EXPIRATION_ISSUE,       ///< Expiration date issues
    CROSS_REFERENCE,        ///< Cross-reference mismatch
    CHECKSUM_MISMATCH       ///< Checksum verification failed
};

/**
 * @brief Integrity issue descriptor
 */
struct IntegrityIssue {
    IssueCategory category;
    IssueSeverity severity;
    std::string target;         ///< Affected volume/dataset
    std::string description;
    std::string suggested_fix;
    bool auto_fixable = false;
    std::chrono::system_clock::time_point detected;
    
    IntegrityIssue() : category(IssueCategory::INVALID_DATA),
                       severity(IssueSeverity::WARNING),
                       detected(std::chrono::system_clock::now()) {}
    
    IntegrityIssue(IssueCategory cat, IssueSeverity sev, 
                   const std::string& tgt, const std::string& desc)
        : category(cat), severity(sev), target(tgt), description(desc),
          detected(std::chrono::system_clock::now()) {}
};

/**
 * @brief Repair action
 */
struct RepairAction {
    std::string target;
    std::string action;
    std::string before;
    std::string after;
    bool applied = false;
    bool success = false;
    std::string error_message;
};

/**
 * @brief Integrity check result
 */
struct IntegrityCheckResult {
    bool passed = true;
    std::chrono::system_clock::time_point check_time;
    std::chrono::milliseconds duration{0};
    
    size_t volumes_checked = 0;
    size_t datasets_checked = 0;
    
    size_t info_count = 0;
    size_t warning_count = 0;
    size_t error_count = 0;
    size_t critical_count = 0;
    
    std::vector<IntegrityIssue> issues;
    std::string checksum;
    
    size_t total_issues() const {
        return info_count + warning_count + error_count + critical_count;
    }
    
    bool has_errors() const {
        return error_count > 0 || critical_count > 0;
    }
    
    std::vector<IntegrityIssue> get_fixable_issues() const {
        std::vector<IntegrityIssue> result;
        for (const auto& issue : issues) {
            if (issue.auto_fixable) {
                result.push_back(issue);
            }
        }
        return result;
    }
};

/**
 * @brief Repair result
 */
struct RepairResult {
    size_t attempted = 0;
    size_t succeeded = 0;
    size_t failed = 0;
    std::vector<RepairAction> actions;
    std::chrono::milliseconds duration{0};
    
    bool all_succeeded() const { return failed == 0; }
};

// ============================================================================
// Integrity Checker
// ============================================================================

/**
 * @brief Comprehensive catalog integrity checker
 */
class IntegrityChecker {
public:
    using VolumeListCallback = std::function<std::vector<TapeVolume>()>;
    using DatasetListCallback = std::function<std::vector<Dataset>()>;
    using VolumeUpdateCallback = std::function<OperationResult(const TapeVolume&)>;
    using DatasetDeleteCallback = std::function<OperationResult(const std::string&)>;
    
    IntegrityChecker() = default;
    
    /// Run full integrity check
    IntegrityCheckResult check_integrity(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Check specific volume
    std::vector<IntegrityIssue> check_volume(const TapeVolume& volume);
    
    /// Check specific dataset
    std::vector<IntegrityIssue> check_dataset(const Dataset& dataset,
        VolumeListCallback get_volumes);
    
    /// Check cross-references
    std::vector<IntegrityIssue> check_cross_references(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Check capacity consistency
    std::vector<IntegrityIssue> check_capacity_consistency(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Check for duplicates
    std::vector<IntegrityIssue> check_duplicates(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Check expiration dates
    std::vector<IntegrityIssue> check_expirations(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Generate catalog checksum
    std::string calculate_checksum(
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Verify checksum
    bool verify_checksum(
        const std::string& expected,
        VolumeListCallback get_volumes,
        DatasetListCallback get_datasets);
    
    /// Auto-repair issues
    RepairResult auto_repair(
        const std::vector<IntegrityIssue>& issues,
        VolumeListCallback get_volumes,
        VolumeUpdateCallback update_volume,
        DatasetDeleteCallback delete_dataset);
    
    /// Generate repair suggestions
    std::vector<std::string> get_repair_suggestions(
        const IntegrityCheckResult& result);
    
    /// Generate integrity report
    std::string generate_report(const IntegrityCheckResult& result);
    
    /// Export issues to file
    OperationResult export_issues(
        const std::vector<IntegrityIssue>& issues,
        const std::string& path);
    
    /// Configuration
    void set_check_checksums(bool enable) { check_checksums_ = enable; }
    void set_verbose(bool enable) { verbose_ = enable; }
    
    /// Utility functions
    static std::string category_to_string(IssueCategory cat);
    static std::string severity_to_string(IssueSeverity sev);
    
private:
    void add_issue(std::vector<IntegrityIssue>& issues,
                   IssueCategory cat, IssueSeverity sev,
                   const std::string& target, const std::string& desc,
                   const std::string& fix = "", bool auto_fix = false);
    
    bool check_checksums_ = true;
    bool verbose_ = false;
};

// ============================================================================
// Implementation
// ============================================================================

inline IntegrityCheckResult IntegrityChecker::check_integrity(
    VolumeListCallback get_volumes,
    DatasetListCallback get_datasets) {
    
    auto start = std::chrono::steady_clock::now();
    IntegrityCheckResult result;
    result.check_time = std::chrono::system_clock::now();
    
    auto volumes = get_volumes();
    auto datasets = get_datasets();
    
    result.volumes_checked = volumes.size();
    result.datasets_checked = datasets.size();
    
    // Check each volume
    for (const auto& vol : volumes) {
        auto vol_issues = check_volume(vol);
        result.issues.insert(result.issues.end(), vol_issues.begin(), vol_issues.end());
    }
    
    // Check each dataset
    for (const auto& ds : datasets) {
        auto ds_issues = check_dataset(ds, get_volumes);
        result.issues.insert(result.issues.end(), ds_issues.begin(), ds_issues.end());
    }
    
    // Check cross-references
    auto xref_issues = check_cross_references(get_volumes, get_datasets);
    result.issues.insert(result.issues.end(), xref_issues.begin(), xref_issues.end());
    
    // Check capacity consistency
    auto cap_issues = check_capacity_consistency(get_volumes, get_datasets);
    result.issues.insert(result.issues.end(), cap_issues.begin(), cap_issues.end());
    
    // Check duplicates
    auto dup_issues = check_duplicates(get_volumes, get_datasets);
    result.issues.insert(result.issues.end(), dup_issues.begin(), dup_issues.end());
    
    // Check expirations
    auto exp_issues = check_expirations(get_volumes, get_datasets);
    result.issues.insert(result.issues.end(), exp_issues.begin(), exp_issues.end());
    
    // Calculate checksum
    if (check_checksums_) {
        result.checksum = calculate_checksum(get_volumes, get_datasets);
    }
    
    // Count by severity
    for (const auto& issue : result.issues) {
        switch (issue.severity) {
            case IssueSeverity::INFO: result.info_count++; break;
            case IssueSeverity::WARNING: result.warning_count++; break;
            case IssueSeverity::ERROR: result.error_count++; break;
            case IssueSeverity::CRITICAL: result.critical_count++; break;
        }
    }
    
    result.passed = !result.has_errors();
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_volume(const TapeVolume& volume) {
    std::vector<IntegrityIssue> issues;
    
    // Check volser
    if (volume.volser.empty()) {
        add_issue(issues, IssueCategory::MISSING_REQUIRED, IssueSeverity::CRITICAL,
            "", "Volume has empty volser", "Delete invalid volume entry", true);
    } else if (volume.volser.length() > 6) {
        add_issue(issues, IssueCategory::INVALID_DATA, IssueSeverity::ERROR,
            volume.volser, "Volser exceeds maximum length of 6 characters");
    }
    
    // Check capacity
    if (volume.used_bytes > volume.capacity_bytes) {
        add_issue(issues, IssueCategory::CAPACITY_MISMATCH, IssueSeverity::ERROR,
            volume.volser, "Used bytes (" + std::to_string(volume.used_bytes) + 
            ") exceeds capacity (" + std::to_string(volume.capacity_bytes) + ")",
            "Recalculate used bytes from datasets", true);
    }
    
    // Check status consistency
    if (volume.status == VolumeStatus::SCRATCH && !volume.datasets.empty()) {
        add_issue(issues, IssueCategory::STATUS_INCONSISTENCY, IssueSeverity::WARNING,
            volume.volser, "Scratch volume has " + std::to_string(volume.datasets.size()) + 
            " datasets", "Change status to PRIVATE", true);
    }
    
    if (volume.status == VolumeStatus::PRIVATE && volume.datasets.empty() && 
        volume.used_bytes == 0) {
        add_issue(issues, IssueCategory::STATUS_INCONSISTENCY, IssueSeverity::INFO,
            volume.volser, "Private volume has no datasets",
            "Consider changing to SCRATCH", false);
    }
    
    // Check dates
    if (volume.expiration_date < volume.creation_date) {
        add_issue(issues, IssueCategory::EXPIRATION_ISSUE, IssueSeverity::WARNING,
            volume.volser, "Expiration date before creation date");
    }
    
    // Check mount count
    if (volume.mount_count < 0) {
        add_issue(issues, IssueCategory::INVALID_DATA, IssueSeverity::WARNING,
            volume.volser, "Negative mount count", "Reset to 0", true);
    }
    
    // Check error count
    if (volume.error_count > 100) {
        add_issue(issues, IssueCategory::INVALID_DATA, IssueSeverity::WARNING,
            volume.volser, "High error count (" + std::to_string(volume.error_count) + 
            ") - consider taking offline");
    }
    
    return issues;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_dataset(
    const Dataset& dataset, VolumeListCallback get_volumes) {
    
    std::vector<IntegrityIssue> issues;
    
    // Check name
    if (dataset.name.empty()) {
        add_issue(issues, IssueCategory::MISSING_REQUIRED, IssueSeverity::CRITICAL,
            "", "Dataset has empty name", "Delete invalid dataset entry", true);
        return issues;
    }
    
    if (dataset.name.length() > 44) {
        add_issue(issues, IssueCategory::INVALID_DATA, IssueSeverity::ERROR,
            dataset.name, "Dataset name exceeds 44 characters");
    }
    
    // Check volume reference
    if (dataset.volser.empty()) {
        add_issue(issues, IssueCategory::MISSING_REQUIRED, IssueSeverity::ERROR,
            dataset.name, "Dataset has no volume reference");
    } else {
        // Verify volume exists
        bool found = false;
        for (const auto& vol : get_volumes()) {
            if (vol.volser == dataset.volser) {
                found = true;
                break;
            }
        }
        if (!found) {
            add_issue(issues, IssueCategory::ORPHAN_DATASET, IssueSeverity::ERROR,
                dataset.name, "References non-existent volume: " + dataset.volser,
                "Delete orphan dataset", true);
        }
    }
    
    // Check dates
    if (dataset.expiration_date < dataset.creation_date) {
        add_issue(issues, IssueCategory::EXPIRATION_ISSUE, IssueSeverity::WARNING,
            dataset.name, "Expiration date before creation date");
    }
    
    return issues;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_cross_references(
    VolumeListCallback get_volumes, DatasetListCallback get_datasets) {
    
    std::vector<IntegrityIssue> issues;
    auto volumes = get_volumes();
    auto datasets = get_datasets();
    
    // Build dataset set
    std::set<std::string> dataset_names;
    for (const auto& ds : datasets) {
        dataset_names.insert(ds.name);
    }
    
    // Check volume dataset lists
    for (const auto& vol : volumes) {
        for (const auto& ds_name : vol.datasets) {
            if (dataset_names.find(ds_name) == dataset_names.end()) {
                add_issue(issues, IssueCategory::ORPHAN_REFERENCE, IssueSeverity::WARNING,
                    vol.volser, "References non-existent dataset: " + ds_name,
                    "Remove from volume's dataset list", true);
            }
        }
    }
    
    // Check dataset volume references
    std::map<std::string, TapeVolume> vol_map;
    for (const auto& vol : volumes) {
        vol_map[vol.volser] = vol;
    }
    
    for (const auto& ds : datasets) {
        auto it = vol_map.find(ds.volser);
        if (it != vol_map.end()) {
            bool found = false;
            for (const auto& name : it->second.datasets) {
                if (name == ds.name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                add_issue(issues, IssueCategory::CROSS_REFERENCE, IssueSeverity::WARNING,
                    ds.name, "Not in volume " + ds.volser + "'s dataset list",
                    "Add to volume's dataset list", true);
            }
        }
    }
    
    return issues;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_capacity_consistency(
    VolumeListCallback get_volumes, DatasetListCallback get_datasets) {
    
    std::vector<IntegrityIssue> issues;
    auto volumes = get_volumes();
    auto datasets = get_datasets();
    
    // Calculate actual used bytes per volume
    std::map<std::string, uint64_t> calculated_usage;
    for (const auto& ds : datasets) {
        calculated_usage[ds.volser] += ds.size_bytes;
    }
    
    for (const auto& vol : volumes) {
        auto it = calculated_usage.find(vol.volser);
        uint64_t calc_used = (it != calculated_usage.end()) ? it->second : 0;
        
        if (calc_used != vol.used_bytes) {
            int64_t diff = static_cast<int64_t>(vol.used_bytes) - static_cast<int64_t>(calc_used);
            add_issue(issues, IssueCategory::CAPACITY_MISMATCH, IssueSeverity::WARNING,
                vol.volser, "Used bytes mismatch: stored=" + std::to_string(vol.used_bytes) +
                " calculated=" + std::to_string(calc_used) + " (diff=" + std::to_string(diff) + ")",
                "Update to calculated value: " + std::to_string(calc_used), true);
        }
    }
    
    return issues;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_duplicates(
    VolumeListCallback get_volumes, DatasetListCallback get_datasets) {
    
    std::vector<IntegrityIssue> issues;
    
    // Check duplicate volumes
    std::set<std::string> seen_volsers;
    for (const auto& vol : get_volumes()) {
        if (seen_volsers.count(vol.volser) > 0) {
            add_issue(issues, IssueCategory::DUPLICATE_ENTRY, IssueSeverity::CRITICAL,
                vol.volser, "Duplicate volume serial");
        }
        seen_volsers.insert(vol.volser);
    }
    
    // Check duplicate datasets
    std::set<std::string> seen_datasets;
    for (const auto& ds : get_datasets()) {
        if (seen_datasets.count(ds.name) > 0) {
            add_issue(issues, IssueCategory::DUPLICATE_ENTRY, IssueSeverity::ERROR,
                ds.name, "Duplicate dataset name");
        }
        seen_datasets.insert(ds.name);
    }
    
    return issues;
}

inline std::vector<IntegrityIssue> IntegrityChecker::check_expirations(
    VolumeListCallback get_volumes, DatasetListCallback get_datasets) {
    
    std::vector<IntegrityIssue> issues;
    auto now = std::chrono::system_clock::now();
    
    for (const auto& vol : get_volumes()) {
        if (vol.status != VolumeStatus::EXPIRED && vol.expiration_date < now) {
            add_issue(issues, IssueCategory::EXPIRATION_ISSUE, IssueSeverity::INFO,
                vol.volser, "Volume is past expiration date but not marked expired",
                "Run expiration processing", false);
        }
    }
    
    for (const auto& ds : get_datasets()) {
        if (ds.status != DatasetStatus::EXPIRED && ds.expiration_date < now) {
            add_issue(issues, IssueCategory::EXPIRATION_ISSUE, IssueSeverity::INFO,
                ds.name, "Dataset is past expiration date but not marked expired",
                "Run expiration processing", false);
        }
    }
    
    return issues;
}

inline std::string IntegrityChecker::calculate_checksum(
    VolumeListCallback get_volumes, DatasetListCallback get_datasets) {
    
    uint32_t checksum = 0;
    
    for (const auto& vol : get_volumes()) {
        for (char c : vol.volser) checksum += static_cast<unsigned char>(c);
        checksum += static_cast<uint32_t>(vol.status);
        checksum += static_cast<uint32_t>(vol.capacity_bytes & 0xFFFFFFFF);
    }
    
    for (const auto& ds : get_datasets()) {
        for (char c : ds.name) checksum += static_cast<unsigned char>(c);
        for (char c : ds.volser) checksum += static_cast<unsigned char>(c);
        checksum += static_cast<uint32_t>(ds.size_bytes & 0xFFFFFFFF);
    }
    
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << checksum;
    return oss.str();
}

inline bool IntegrityChecker::verify_checksum(
    const std::string& expected,
    VolumeListCallback get_volumes,
    DatasetListCallback get_datasets) {
    
    return calculate_checksum(get_volumes, get_datasets) == expected;
}

inline std::string IntegrityChecker::generate_report(const IntegrityCheckResult& result) {
    std::ostringstream oss;
    
    oss << "\n=== INTEGRITY CHECK REPORT ===\n";
    oss << "Check Time: " << format_time(result.check_time) << "\n";
    oss << "Duration: " << result.duration.count() << " ms\n";
    oss << "Status: " << (result.passed ? "PASSED" : "FAILED") << "\n\n";
    
    oss << "Checked:\n";
    oss << "  Volumes:  " << result.volumes_checked << "\n";
    oss << "  Datasets: " << result.datasets_checked << "\n\n";
    
    oss << "Issues Found:\n";
    oss << "  Info:     " << result.info_count << "\n";
    oss << "  Warning:  " << result.warning_count << "\n";
    oss << "  Error:    " << result.error_count << "\n";
    oss << "  Critical: " << result.critical_count << "\n";
    oss << "  Total:    " << result.total_issues() << "\n\n";
    
    if (!result.checksum.empty()) {
        oss << "Catalog Checksum: " << result.checksum << "\n\n";
    }
    
    if (!result.issues.empty()) {
        oss << "Issue Details:\n";
        oss << std::string(60, '-') << "\n";
        
        for (const auto& issue : result.issues) {
            oss << "[" << severity_to_string(issue.severity) << "] "
                << category_to_string(issue.category) << "\n";
            oss << "  Target: " << issue.target << "\n";
            oss << "  " << issue.description << "\n";
            if (!issue.suggested_fix.empty()) {
                oss << "  Fix: " << issue.suggested_fix;
                if (issue.auto_fixable) oss << " [AUTO-FIX AVAILABLE]";
                oss << "\n";
            }
            oss << "\n";
        }
    }
    
    return oss.str();
}

inline void IntegrityChecker::add_issue(
    std::vector<IntegrityIssue>& issues,
    IssueCategory cat, IssueSeverity sev,
    const std::string& target, const std::string& desc,
    const std::string& fix, bool auto_fix) {
    
    IntegrityIssue issue(cat, sev, target, desc);
    issue.suggested_fix = fix;
    issue.auto_fixable = auto_fix;
    issues.push_back(issue);
}

inline std::string IntegrityChecker::category_to_string(IssueCategory cat) {
    switch (cat) {
        case IssueCategory::ORPHAN_DATASET: return "ORPHAN_DATASET";
        case IssueCategory::ORPHAN_REFERENCE: return "ORPHAN_REFERENCE";
        case IssueCategory::CAPACITY_MISMATCH: return "CAPACITY_MISMATCH";
        case IssueCategory::STATUS_INCONSISTENCY: return "STATUS_INCONSISTENCY";
        case IssueCategory::DUPLICATE_ENTRY: return "DUPLICATE_ENTRY";
        case IssueCategory::INVALID_DATA: return "INVALID_DATA";
        case IssueCategory::MISSING_REQUIRED: return "MISSING_REQUIRED";
        case IssueCategory::EXPIRATION_ISSUE: return "EXPIRATION_ISSUE";
        case IssueCategory::CROSS_REFERENCE: return "CROSS_REFERENCE";
        case IssueCategory::CHECKSUM_MISMATCH: return "CHECKSUM_MISMATCH";
        default: return "UNKNOWN";
    }
}

inline std::string IntegrityChecker::severity_to_string(IssueSeverity sev) {
    switch (sev) {
        case IssueSeverity::INFO: return "INFO";
        case IssueSeverity::WARNING: return "WARNING";
        case IssueSeverity::ERROR: return "ERROR";
        case IssueSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace tms

#endif // TMS_INTEGRITY_H
