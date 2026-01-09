/**
 * @file tms_groups.h
 * @brief TMS Tape Management System - Volume Group Management
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides volume grouping functionality for organizing tape volumes
 * into logical collections with group-level operations.
 */

#ifndef TMS_GROUPS_H
#define TMS_GROUPS_H

#include "tms_types.h"
#include "error_codes.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <chrono>
#include <optional>
#include <mutex>
#include <functional>

namespace tms {

// ============================================================================
// Volume Group Types
// ============================================================================

/**
 * @brief Volume group definition
 */
struct VolumeGroup {
    std::string name;                                   ///< Group name
    std::string description;                            ///< Group description
    std::set<std::string> volumes;                      ///< Member volume serials
    std::string owner;                                  ///< Group owner
    std::chrono::system_clock::time_point created;      ///< Creation time
    std::chrono::system_clock::time_point modified;     ///< Last modification
    std::set<std::string> tags;                         ///< Group tags
    bool read_only = false;                             ///< Prevent modifications
    size_t max_volumes = 0;                             ///< Max volumes (0 = unlimited)
    std::string default_pool;                           ///< Default pool for new volumes
    std::string retention_policy;                       ///< Associated retention policy
    
    size_t size() const { return volumes.size(); }
    bool empty() const { return volumes.empty(); }
    bool contains(const std::string& volser) const {
        return volumes.find(volser) != volumes.end();
    }
    bool is_full() const {
        return max_volumes > 0 && volumes.size() >= max_volumes;
    }
};

/**
 * @brief Group statistics
 */
struct GroupStatistics {
    std::string group_name;
    size_t total_volumes = 0;
    size_t scratch_volumes = 0;
    size_t private_volumes = 0;
    size_t mounted_volumes = 0;
    size_t expired_volumes = 0;
    uint64_t total_capacity = 0;
    uint64_t used_capacity = 0;
    size_t total_datasets = 0;
    
    double get_utilization() const {
        return total_capacity > 0 ? 
            (100.0 * static_cast<double>(used_capacity) / static_cast<double>(total_capacity)) : 0.0;
    }
};

/**
 * @brief Group operation result
 */
struct GroupOperationResult {
    size_t total = 0;
    size_t succeeded = 0;
    size_t failed = 0;
    std::vector<std::pair<std::string, std::string>> failures;
    std::chrono::milliseconds duration{0};
    
    bool all_succeeded() const { return failed == 0; }
    double success_rate() const {
        return total > 0 ? (100.0 * static_cast<double>(succeeded) / static_cast<double>(total)) : 0.0;
    }
};

// ============================================================================
// Volume Group Manager
// ============================================================================

/**
 * @brief Manages volume groups
 */
class VolumeGroupManager {
public:
    using VolumeCallback = std::function<OperationResult(const std::string& volser)>;
    using VolumeInfoCallback = std::function<std::optional<TapeVolume>(const std::string& volser)>;
    
    VolumeGroupManager() = default;
    
    // Group lifecycle
    OperationResult create_group(const VolumeGroup& group);
    OperationResult delete_group(const std::string& name, bool force = false);
    OperationResult update_group(const VolumeGroup& group);
    Result<VolumeGroup> get_group(const std::string& name) const;
    std::vector<VolumeGroup> list_groups() const;
    bool group_exists(const std::string& name) const;
    size_t group_count() const;
    
    // Volume membership
    OperationResult add_volume(const std::string& group_name, const std::string& volser);
    OperationResult remove_volume(const std::string& group_name, const std::string& volser);
    OperationResult add_volumes(const std::string& group_name, const std::vector<std::string>& volsers);
    OperationResult remove_volumes(const std::string& group_name, const std::vector<std::string>& volsers);
    std::vector<std::string> get_volumes(const std::string& group_name) const;
    std::vector<std::string> get_groups_for_volume(const std::string& volser) const;
    
    // Group operations (bulk operations on all volumes in group)
    GroupOperationResult scratch_group(const std::string& name, VolumeCallback scratch_fn);
    GroupOperationResult tag_group(const std::string& name, const std::string& tag, VolumeCallback tag_fn);
    GroupOperationResult set_group_offline(const std::string& name, VolumeCallback offline_fn);
    GroupOperationResult set_group_online(const std::string& name, VolumeCallback online_fn);
    
    // Group queries
    std::vector<VolumeGroup> find_by_owner(const std::string& owner) const;
    std::vector<VolumeGroup> find_by_tag(const std::string& tag) const;
    GroupStatistics get_group_statistics(const std::string& name, 
                                          VolumeInfoCallback get_volume) const;
    
    // Tagging
    OperationResult add_group_tag(const std::string& group_name, const std::string& tag);
    OperationResult remove_group_tag(const std::string& group_name, const std::string& tag);
    std::set<std::string> get_all_group_tags() const;
    
    // Persistence
    OperationResult save_groups(const std::string& path) const;
    OperationResult load_groups(const std::string& path);
    
    // Validation
    std::vector<std::string> validate_group(const std::string& name,
                                             VolumeInfoCallback get_volume) const;
    
private:
    bool validate_group_name(const std::string& name) const;
    
    mutable std::mutex mutex_;
    std::map<std::string, VolumeGroup> groups_;
    std::map<std::string, std::set<std::string>> volume_to_groups_; // reverse index
};

// ============================================================================
// Implementation
// ============================================================================

inline OperationResult VolumeGroupManager::create_group(const VolumeGroup& group) {
    if (!validate_group_name(group.name)) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Invalid group name");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (groups_.count(group.name) > 0) {
        return OperationResult::err(TMSError::VOLUME_ALREADY_EXISTS, "Group already exists: " + group.name);
    }
    
    VolumeGroup g = group;
    g.created = std::chrono::system_clock::now();
    g.modified = g.created;
    
    groups_[g.name] = g;
    
    // Update reverse index
    for (const auto& volser : g.volumes) {
        volume_to_groups_[volser].insert(g.name);
    }
    
    return OperationResult::ok();
}

inline OperationResult VolumeGroupManager::delete_group(const std::string& name, bool force) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Group not found: " + name);
    }
    
    if (!force && !it->second.volumes.empty()) {
        return OperationResult::err(TMSError::VOLUME_HAS_DATASETS, 
            "Group has " + std::to_string(it->second.volumes.size()) + " volumes");
    }
    
    // Update reverse index
    for (const auto& volser : it->second.volumes) {
        volume_to_groups_[volser].erase(name);
        if (volume_to_groups_[volser].empty()) {
            volume_to_groups_.erase(volser);
        }
    }
    
    groups_.erase(it);
    return OperationResult::ok();
}

inline OperationResult VolumeGroupManager::update_group(const VolumeGroup& group) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(group.name);
    if (it == groups_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Group not found: " + group.name);
    }
    
    if (it->second.read_only) {
        return OperationResult::err(TMSError::ACCESS_DENIED, "Group is read-only");
    }
    
    // Update reverse index for volume changes
    for (const auto& volser : it->second.volumes) {
        if (group.volumes.find(volser) == group.volumes.end()) {
            volume_to_groups_[volser].erase(group.name);
            if (volume_to_groups_[volser].empty()) {
                volume_to_groups_.erase(volser);
            }
        }
    }
    for (const auto& volser : group.volumes) {
        volume_to_groups_[volser].insert(group.name);
    }
    
    VolumeGroup updated = group;
    updated.created = it->second.created;
    updated.modified = std::chrono::system_clock::now();
    it->second = updated;
    
    return OperationResult::ok();
}

inline Result<VolumeGroup> VolumeGroupManager::get_group(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(name);
    if (it == groups_.end()) {
        return Result<VolumeGroup>::err(TMSError::VOLUME_NOT_FOUND, "Group not found: " + name);
    }
    
    return Result<VolumeGroup>::ok(it->second);
}

inline std::vector<VolumeGroup> VolumeGroupManager::list_groups() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VolumeGroup> result;
    result.reserve(groups_.size());
    for (const auto& [name, group] : groups_) {
        result.push_back(group);
    }
    return result;
}

inline bool VolumeGroupManager::group_exists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return groups_.count(name) > 0;
}

inline size_t VolumeGroupManager::group_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return groups_.size();
}

inline OperationResult VolumeGroupManager::add_volume(const std::string& group_name, 
                                                       const std::string& volser) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(group_name);
    if (it == groups_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Group not found: " + group_name);
    }
    
    if (it->second.read_only) {
        return OperationResult::err(TMSError::ACCESS_DENIED, "Group is read-only");
    }
    
    if (it->second.is_full()) {
        return OperationResult::err(TMSError::VOLUME_LIMIT_REACHED, "Group is full");
    }
    
    it->second.volumes.insert(volser);
    it->second.modified = std::chrono::system_clock::now();
    volume_to_groups_[volser].insert(group_name);
    
    return OperationResult::ok();
}

inline OperationResult VolumeGroupManager::remove_volume(const std::string& group_name,
                                                          const std::string& volser) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(group_name);
    if (it == groups_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Group not found: " + group_name);
    }
    
    if (it->second.read_only) {
        return OperationResult::err(TMSError::ACCESS_DENIED, "Group is read-only");
    }
    
    it->second.volumes.erase(volser);
    it->second.modified = std::chrono::system_clock::now();
    
    volume_to_groups_[volser].erase(group_name);
    if (volume_to_groups_[volser].empty()) {
        volume_to_groups_.erase(volser);
    }
    
    return OperationResult::ok();
}

inline std::vector<std::string> VolumeGroupManager::get_volumes(const std::string& group_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = groups_.find(group_name);
    if (it == groups_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.volumes.begin(), it->second.volumes.end());
}

inline std::vector<std::string> VolumeGroupManager::get_groups_for_volume(const std::string& volser) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = volume_to_groups_.find(volser);
    if (it == volume_to_groups_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

inline GroupOperationResult VolumeGroupManager::scratch_group(const std::string& name,
                                                               VolumeCallback scratch_fn) {
    auto start = std::chrono::steady_clock::now();
    GroupOperationResult result;
    
    auto volumes = get_volumes(name);
    result.total = volumes.size();
    
    for (const auto& volser : volumes) {
        auto op = scratch_fn(volser);
        if (op.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(volser, op.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

inline std::vector<VolumeGroup> VolumeGroupManager::find_by_owner(const std::string& owner) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VolumeGroup> result;
    for (const auto& [name, group] : groups_) {
        if (group.owner == owner) {
            result.push_back(group);
        }
    }
    return result;
}

inline std::vector<VolumeGroup> VolumeGroupManager::find_by_tag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<VolumeGroup> result;
    for (const auto& [name, group] : groups_) {
        if (group.tags.count(tag) > 0) {
            result.push_back(group);
        }
    }
    return result;
}

inline GroupStatistics VolumeGroupManager::get_group_statistics(const std::string& name,
                                                                  VolumeInfoCallback get_volume) const {
    GroupStatistics stats;
    stats.group_name = name;
    
    auto volumes = get_volumes(name);
    stats.total_volumes = volumes.size();
    
    for (const auto& volser : volumes) {
        auto vol_opt = get_volume(volser);
        if (!vol_opt.has_value()) continue;
        
        const auto& vol = vol_opt.value();
        stats.total_capacity += vol.capacity_bytes;
        stats.used_capacity += vol.used_bytes;
        stats.total_datasets += vol.datasets.size();
        
        switch (vol.status) {
            case VolumeStatus::SCRATCH: stats.scratch_volumes++; break;
            case VolumeStatus::PRIVATE: stats.private_volumes++; break;
            case VolumeStatus::MOUNTED: stats.mounted_volumes++; break;
            case VolumeStatus::EXPIRED: stats.expired_volumes++; break;
            default: break;
        }
    }
    
    return stats;
}

inline bool VolumeGroupManager::validate_group_name(const std::string& name) const {
    if (name.empty() || name.length() > 32) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

inline OperationResult VolumeGroupManager::save_groups(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open file: " + path);
    }
    
    file << "# TMS Volume Groups\n";
    file << "# Generated: " << get_timestamp() << "\n\n";
    
    for (const auto& [name, group] : groups_) {
        file << "[GROUP:" << name << "]\n";
        file << "description=" << group.description << "\n";
        file << "owner=" << group.owner << "\n";
        file << "read_only=" << (group.read_only ? "1" : "0") << "\n";
        file << "max_volumes=" << group.max_volumes << "\n";
        file << "default_pool=" << group.default_pool << "\n";
        file << "retention_policy=" << group.retention_policy << "\n";
        file << "volumes=";
        bool first = true;
        for (const auto& v : group.volumes) {
            if (!first) file << ",";
            file << v;
            first = false;
        }
        file << "\n";
        file << "tags=";
        first = true;
        for (const auto& t : group.tags) {
            if (!first) file << ",";
            file << t;
            first = false;
        }
        file << "\n\n";
    }
    
    return OperationResult::ok();
}

inline OperationResult VolumeGroupManager::load_groups(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Cannot open file: " + path);
    }
    
    groups_.clear();
    volume_to_groups_.clear();
    
    std::string line;
    VolumeGroup current;
    bool in_group = false;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        if (line.substr(0, 7) == "[GROUP:") {
            if (in_group && !current.name.empty()) {
                groups_[current.name] = current;
                for (const auto& v : current.volumes) {
                    volume_to_groups_[v].insert(current.name);
                }
            }
            current = VolumeGroup{};
            current.name = line.substr(7, line.length() - 8);
            in_group = true;
            continue;
        }
        
        if (!in_group) continue;
        
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        
        if (key == "description") current.description = value;
        else if (key == "owner") current.owner = value;
        else if (key == "read_only") current.read_only = (value == "1");
        else if (key == "max_volumes") current.max_volumes = std::stoull(value);
        else if (key == "default_pool") current.default_pool = value;
        else if (key == "retention_policy") current.retention_policy = value;
        else if (key == "volumes") {
            std::istringstream iss(value);
            std::string v;
            while (std::getline(iss, v, ',')) {
                if (!v.empty()) current.volumes.insert(v);
            }
        }
        else if (key == "tags") {
            std::istringstream iss(value);
            std::string t;
            while (std::getline(iss, t, ',')) {
                if (!t.empty()) current.tags.insert(t);
            }
        }
    }
    
    if (in_group && !current.name.empty()) {
        groups_[current.name] = current;
        for (const auto& v : current.volumes) {
            volume_to_groups_[v].insert(current.name);
        }
    }
    
    return OperationResult::ok();
}

} // namespace tms

#endif // TMS_GROUPS_H
