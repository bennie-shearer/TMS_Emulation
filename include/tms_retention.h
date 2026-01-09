/**
 * @file tms_retention.h
 * @brief TMS Tape Management System - Retention Policy Management
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides configurable retention policies for automated lifecycle management
 * of volumes and datasets.
 */

#ifndef TMS_RETENTION_H
#define TMS_RETENTION_H

#include "tms_types.h"
#include "error_codes.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>
#include <mutex>
#include <functional>

namespace tms {

// ============================================================================
// Retention Policy Types
// ============================================================================

/**
 * @brief Retention action to take when policy expires
 */
enum class RetentionAction {
    EXPIRE,             ///< Mark as expired
    DELETE,             ///< Delete immediately
    MIGRATE,            ///< Migrate to secondary storage
    ARCHIVE,            ///< Archive to vault
    SCRATCH,            ///< Return to scratch pool
    NOTIFY              ///< Notify only, no automatic action
};

/**
 * @brief Retention period unit
 */
enum class RetentionUnit {
    DAYS,
    WEEKS,
    MONTHS,
    YEARS,
    FOREVER             ///< Never expires
};

/**
 * @brief Retention policy definition
 */
struct RetentionPolicy {
    std::string name;                           ///< Policy name
    std::string description;                    ///< Policy description
    int retention_value = 30;                   ///< Retention period value
    RetentionUnit retention_unit = RetentionUnit::DAYS;  ///< Period unit
    RetentionAction action = RetentionAction::EXPIRE;    ///< Action on expiry
    bool active = true;                         ///< Is policy active
    int warning_days = 7;                       ///< Days before expiry to warn
    std::string owner;                          ///< Policy owner
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    std::set<std::string> tags;                 ///< Policy tags
    std::string notification_email;             ///< Email for notifications
    bool apply_to_volumes = true;               ///< Apply to volumes
    bool apply_to_datasets = true;              ///< Apply to datasets
    std::string pool_filter;                    ///< Only apply to specific pool
    std::string owner_filter;                   ///< Only apply to specific owner
    
    /// Calculate expiration date from creation date
    std::chrono::system_clock::time_point calculate_expiration(
        const std::chrono::system_clock::time_point& creation) const {
        
        if (retention_unit == RetentionUnit::FOREVER) {
            return std::chrono::system_clock::time_point::max();
        }
        
        using namespace std::chrono;
        int days = 0;
        switch (retention_unit) {
            case RetentionUnit::DAYS: days = retention_value; break;
            case RetentionUnit::WEEKS: days = retention_value * 7; break;
            case RetentionUnit::MONTHS: days = retention_value * 30; break;
            case RetentionUnit::YEARS: days = retention_value * 365; break;
            default: break;
        }
        return creation + hours(24 * days);
    }
    
    /// Check if warning period is active
    bool is_in_warning_period(const std::chrono::system_clock::time_point& expiry) const {
        auto now = std::chrono::system_clock::now();
        auto warning_start = expiry - std::chrono::hours(24 * warning_days);
        return now >= warning_start && now < expiry;
    }
};

/**
 * @brief Policy application result
 */
struct PolicyApplicationResult {
    std::string policy_name;
    size_t volumes_processed = 0;
    size_t datasets_processed = 0;
    size_t volumes_expired = 0;
    size_t datasets_expired = 0;
    size_t volumes_warned = 0;
    size_t datasets_warned = 0;
    size_t errors = 0;
    std::vector<std::pair<std::string, std::string>> warnings;
    std::chrono::milliseconds duration{0};
};

// ============================================================================
// Retention Policy Manager
// ============================================================================

/**
 * @brief Manages retention policies
 */
class RetentionPolicyManager {
public:
    using VolumeProcessor = std::function<OperationResult(const std::string& volser, RetentionAction action)>;
    using DatasetProcessor = std::function<OperationResult(const std::string& name, RetentionAction action)>;
    using VolumeListCallback = std::function<std::vector<TapeVolume>()>;
    using DatasetListCallback = std::function<std::vector<Dataset>()>;
    
    RetentionPolicyManager() = default;
    
    // Policy lifecycle
    OperationResult create_policy(const RetentionPolicy& policy);
    OperationResult delete_policy(const std::string& name);
    OperationResult update_policy(const RetentionPolicy& policy);
    Result<RetentionPolicy> get_policy(const std::string& name) const;
    std::vector<RetentionPolicy> list_policies(bool active_only = false) const;
    bool policy_exists(const std::string& name) const;
    size_t policy_count() const;
    
    // Policy application
    OperationResult assign_policy(const std::string& target, const std::string& policy_name);
    OperationResult unassign_policy(const std::string& target);
    std::optional<std::string> get_assigned_policy(const std::string& target) const;
    std::vector<std::string> get_targets_with_policy(const std::string& policy_name) const;
    
    // Policy processing
    PolicyApplicationResult process_policy(const std::string& policy_name,
                                            VolumeListCallback get_volumes,
                                            DatasetListCallback get_datasets,
                                            VolumeProcessor process_volume,
                                            DatasetProcessor process_dataset,
                                            bool dry_run = false);
    
    std::vector<PolicyApplicationResult> process_all_policies(
                                            VolumeListCallback get_volumes,
                                            DatasetListCallback get_datasets,
                                            VolumeProcessor process_volume,
                                            DatasetProcessor process_dataset,
                                            bool dry_run = false);
    
    // Expiration queries
    std::vector<std::string> get_expiring_volumes(
        const std::string& policy_name,
        VolumeListCallback get_volumes,
        std::chrono::hours within = std::chrono::hours(24 * 7)) const;
    
    std::vector<std::string> get_expiring_datasets(
        const std::string& policy_name,
        DatasetListCallback get_datasets,
        std::chrono::hours within = std::chrono::hours(24 * 7)) const;
    
    // Persistence
    OperationResult save_policies(const std::string& path) const;
    OperationResult load_policies(const std::string& path);
    
    // Validation
    std::vector<std::string> validate_policy(const RetentionPolicy& policy) const;
    
    // Utility
    static std::string action_to_string(RetentionAction action);
    static RetentionAction string_to_action(const std::string& str);
    static std::string unit_to_string(RetentionUnit unit);
    static RetentionUnit string_to_unit(const std::string& str);
    
private:
    bool validate_policy_name(const std::string& name) const;
    bool matches_policy_filter(const RetentionPolicy& policy, const std::string& pool,
                                const std::string& owner) const;
    
    mutable std::mutex mutex_;
    std::map<std::string, RetentionPolicy> policies_;
    std::map<std::string, std::string> target_policies_;  // target -> policy mapping
};

// ============================================================================
// Implementation
// ============================================================================

inline OperationResult RetentionPolicyManager::create_policy(const RetentionPolicy& policy) {
    if (!validate_policy_name(policy.name)) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Invalid policy name");
    }
    
    auto errors = validate_policy(policy);
    if (!errors.empty()) {
        return OperationResult::err(TMSError::VALIDATION_FAILED, errors[0]);
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (policies_.count(policy.name) > 0) {
        return OperationResult::err(TMSError::VOLUME_ALREADY_EXISTS, 
            "Policy already exists: " + policy.name);
    }
    
    RetentionPolicy p = policy;
    p.created = std::chrono::system_clock::now();
    p.modified = p.created;
    policies_[p.name] = p;
    
    return OperationResult::ok();
}

inline OperationResult RetentionPolicyManager::delete_policy(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = policies_.find(name);
    if (it == policies_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Policy not found: " + name);
    }
    
    // Remove all target assignments for this policy
    for (auto tp_it = target_policies_.begin(); tp_it != target_policies_.end();) {
        if (tp_it->second == name) {
            tp_it = target_policies_.erase(tp_it);
        } else {
            ++tp_it;
        }
    }
    
    policies_.erase(it);
    return OperationResult::ok();
}

inline OperationResult RetentionPolicyManager::update_policy(const RetentionPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = policies_.find(policy.name);
    if (it == policies_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Policy not found: " + policy.name);
    }
    
    RetentionPolicy p = policy;
    p.created = it->second.created;
    p.modified = std::chrono::system_clock::now();
    it->second = p;
    
    return OperationResult::ok();
}

inline Result<RetentionPolicy> RetentionPolicyManager::get_policy(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = policies_.find(name);
    if (it == policies_.end()) {
        return Result<RetentionPolicy>::err(TMSError::VOLUME_NOT_FOUND, "Policy not found: " + name);
    }
    
    return Result<RetentionPolicy>::ok(it->second);
}

inline std::vector<RetentionPolicy> RetentionPolicyManager::list_policies(bool active_only) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<RetentionPolicy> result;
    for (const auto& [name, policy] : policies_) {
        if (!active_only || policy.active) {
            result.push_back(policy);
        }
    }
    return result;
}

inline bool RetentionPolicyManager::policy_exists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policies_.count(name) > 0;
}

inline size_t RetentionPolicyManager::policy_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return policies_.size();
}

inline OperationResult RetentionPolicyManager::assign_policy(const std::string& target,
                                                              const std::string& policy_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (policies_.count(policy_name) == 0) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Policy not found: " + policy_name);
    }
    
    target_policies_[target] = policy_name;
    return OperationResult::ok();
}

inline OperationResult RetentionPolicyManager::unassign_policy(const std::string& target) {
    std::lock_guard<std::mutex> lock(mutex_);
    target_policies_.erase(target);
    return OperationResult::ok();
}

inline std::optional<std::string> RetentionPolicyManager::get_assigned_policy(
    const std::string& target) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = target_policies_.find(target);
    if (it == target_policies_.end()) {
        return std::nullopt;
    }
    return it->second;
}

inline std::vector<std::string> RetentionPolicyManager::validate_policy(
    const RetentionPolicy& policy) const {
    std::vector<std::string> errors;
    
    if (policy.name.empty()) {
        errors.push_back("Policy name cannot be empty");
    }
    if (policy.retention_value < 0) {
        errors.push_back("Retention value cannot be negative");
    }
    if (policy.warning_days < 0) {
        errors.push_back("Warning days cannot be negative");
    }
    if (policy.warning_days > policy.retention_value && 
        policy.retention_unit == RetentionUnit::DAYS) {
        errors.push_back("Warning period cannot exceed retention period");
    }
    
    return errors;
}

inline bool RetentionPolicyManager::validate_policy_name(const std::string& name) const {
    if (name.empty() || name.length() > 32) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

inline std::string RetentionPolicyManager::action_to_string(RetentionAction action) {
    switch (action) {
        case RetentionAction::EXPIRE: return "EXPIRE";
        case RetentionAction::DELETE: return "DELETE";
        case RetentionAction::MIGRATE: return "MIGRATE";
        case RetentionAction::ARCHIVE: return "ARCHIVE";
        case RetentionAction::SCRATCH: return "SCRATCH";
        case RetentionAction::NOTIFY: return "NOTIFY";
        default: return "UNKNOWN";
    }
}

inline RetentionAction RetentionPolicyManager::string_to_action(const std::string& str) {
    if (str == "EXPIRE") return RetentionAction::EXPIRE;
    if (str == "DELETE") return RetentionAction::DELETE;
    if (str == "MIGRATE") return RetentionAction::MIGRATE;
    if (str == "ARCHIVE") return RetentionAction::ARCHIVE;
    if (str == "SCRATCH") return RetentionAction::SCRATCH;
    if (str == "NOTIFY") return RetentionAction::NOTIFY;
    return RetentionAction::EXPIRE;
}

inline std::string RetentionPolicyManager::unit_to_string(RetentionUnit unit) {
    switch (unit) {
        case RetentionUnit::DAYS: return "DAYS";
        case RetentionUnit::WEEKS: return "WEEKS";
        case RetentionUnit::MONTHS: return "MONTHS";
        case RetentionUnit::YEARS: return "YEARS";
        case RetentionUnit::FOREVER: return "FOREVER";
        default: return "DAYS";
    }
}

inline RetentionUnit RetentionPolicyManager::string_to_unit(const std::string& str) {
    if (str == "DAYS") return RetentionUnit::DAYS;
    if (str == "WEEKS") return RetentionUnit::WEEKS;
    if (str == "MONTHS") return RetentionUnit::MONTHS;
    if (str == "YEARS") return RetentionUnit::YEARS;
    if (str == "FOREVER") return RetentionUnit::FOREVER;
    return RetentionUnit::DAYS;
}

inline OperationResult RetentionPolicyManager::save_policies(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open file: " + path);
    }
    
    file << "# TMS Retention Policies\n";
    file << "# Generated: " << get_timestamp() << "\n\n";
    
    for (const auto& [name, policy] : policies_) {
        file << "[POLICY:" << name << "]\n";
        file << "description=" << policy.description << "\n";
        file << "retention_value=" << policy.retention_value << "\n";
        file << "retention_unit=" << unit_to_string(policy.retention_unit) << "\n";
        file << "action=" << action_to_string(policy.action) << "\n";
        file << "active=" << (policy.active ? "1" : "0") << "\n";
        file << "warning_days=" << policy.warning_days << "\n";
        file << "owner=" << policy.owner << "\n";
        file << "apply_to_volumes=" << (policy.apply_to_volumes ? "1" : "0") << "\n";
        file << "apply_to_datasets=" << (policy.apply_to_datasets ? "1" : "0") << "\n";
        file << "pool_filter=" << policy.pool_filter << "\n";
        file << "owner_filter=" << policy.owner_filter << "\n";
        file << "\n";
    }
    
    file << "# Target Assignments\n";
    for (const auto& [target, policy_name] : target_policies_) {
        file << "ASSIGN|" << target << "|" << policy_name << "\n";
    }
    
    return OperationResult::ok();
}

inline OperationResult RetentionPolicyManager::load_policies(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Cannot open file: " + path);
    }
    
    policies_.clear();
    target_policies_.clear();
    
    std::string line;
    RetentionPolicy current;
    bool in_policy = false;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (line.substr(0, 8) == "[POLICY:") {
            if (in_policy && !current.name.empty()) {
                policies_[current.name] = current;
            }
            current = RetentionPolicy{};
            current.name = line.substr(8, line.length() - 9);
            in_policy = true;
            continue;
        }
        
        if (line.substr(0, 7) == "ASSIGN|") {
            auto parts = line.substr(7);
            size_t sep = parts.find('|');
            if (sep != std::string::npos) {
                target_policies_[parts.substr(0, sep)] = parts.substr(sep + 1);
            }
            continue;
        }
        
        if (!in_policy) continue;
        
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        
        if (key == "description") current.description = value;
        else if (key == "retention_value") current.retention_value = std::stoi(value);
        else if (key == "retention_unit") current.retention_unit = string_to_unit(value);
        else if (key == "action") current.action = string_to_action(value);
        else if (key == "active") current.active = (value == "1");
        else if (key == "warning_days") current.warning_days = std::stoi(value);
        else if (key == "owner") current.owner = value;
        else if (key == "apply_to_volumes") current.apply_to_volumes = (value == "1");
        else if (key == "apply_to_datasets") current.apply_to_datasets = (value == "1");
        else if (key == "pool_filter") current.pool_filter = value;
        else if (key == "owner_filter") current.owner_filter = value;
    }
    
    if (in_policy && !current.name.empty()) {
        policies_[current.name] = current;
    }
    
    return OperationResult::ok();
}

} // namespace tms

#endif // TMS_RETENTION_H
