/**
 * @file tms_tape_mgmt.cpp
 * @brief TMS Tape Management System - Core Implementation
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#include "tms_tape_mgmt.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <functional>
#include <cmath>

namespace tms {

namespace fs = std::filesystem;

// ============================================================================
// SystemStatistics Implementation
// ============================================================================

std::string SystemStatistics::get_uptime() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - uptime_start);
    return format_duration(uptime);
}

// ============================================================================
// AuditLog Implementation
// ============================================================================

OperationResult AuditLog::export_to_file(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open audit file: " + path);
    }
    
    file << "Timestamp,Operation,User,Target,Details,Success\n";
    for (const auto& rec : records_) {
        file << format_time(rec.timestamp) << ","
             << rec.operation << ","
             << rec.user << ","
             << rec.target << ","
             << "\"" << rec.details << "\","
             << (rec.success ? "Y" : "N") << "\n";
    }
    
    return OperationResult::ok();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

TMSSystem::TMSSystem(const std::string& data_directory)
    : data_directory_(data_directory),
      start_time_(std::chrono::steady_clock::now()) {
    
    volume_catalog_path_ = data_directory_ + PATH_SEP_STR + "volumes.dat";
    dataset_catalog_path_ = data_directory_ + PATH_SEP_STR + "datasets.dat";
    
    ensure_directory_exists(data_directory_);
    load_catalog();
    
    TMS_LOG_INFO("TMSSystem", "TMS System initialized v" + std::string(VERSION_STRING));
}

TMSSystem::~TMSSystem() {
    save_catalog();
    TMS_LOG_INFO("TMSSystem", "TMS System shutdown complete");
}

bool TMSSystem::ensure_directory_exists(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return fs::create_directories(path);
        }
        return true;
    } catch (const std::exception& e) {
        TMS_LOG_ERROR("TMSSystem", "Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

void TMSSystem::rebuild_indices() {
    // Clear all indices
    volume_owner_index_.clear();
    volume_pool_index_.clear();
    volume_tag_index_.clear();
    dataset_owner_index_.clear();
    dataset_tag_index_.clear();
    
    // Rebuild volume indices
    for (const auto& [volser, vol] : volumes_) {
        volume_owner_index_.add(vol.owner, volser);
        volume_pool_index_.add(vol.pool, volser);
        for (const auto& tag : vol.tags) {
            volume_tag_index_.add(tag, volser);
        }
    }
    
    // Rebuild dataset indices
    for (const auto& [name, ds] : datasets_) {
        dataset_owner_index_.add(ds.owner, name);
        for (const auto& tag : ds.tags) {
            dataset_tag_index_.add(tag, name);
        }
    }
}

// ============================================================================
// Volume Management
// ============================================================================

OperationResult TMSSystem::add_volume(const TapeVolume& volume) {
    if (!validate_volser(volume.volser)) {
        return OperationResult::err(TMSError::INVALID_VOLSER, "Invalid volume serial: " + volume.volser);
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    if (volumes_.size() >= Configuration::instance().get_max_volumes()) {
        return OperationResult::err(TMSError::VOLUME_LIMIT_REACHED, "Maximum volume limit reached");
    }
    
    if (volumes_.count(volume.volser) > 0) {
        return OperationResult::err(TMSError::VOLUME_ALREADY_EXISTS, "Volume already exists: " + volume.volser);
    }
    
    TapeVolume vol = volume;
    if (vol.creation_date == std::chrono::system_clock::time_point{}) {
        vol.creation_date = std::chrono::system_clock::now();
    }
    if (vol.expiration_date == std::chrono::system_clock::time_point{}) {
        vol.expiration_date = vol.creation_date + std::chrono::hours(24 * 365);
    }
    if (vol.capacity_bytes == 0) {
        vol.capacity_bytes = get_density_capacity(vol.density);
    }
    
    // v3.2.0: Calculate initial health score
    vol.health_score = calculate_health_score(vol);
    vol.last_health_check = std::chrono::system_clock::now();
    
    // Add to primary storage
    volumes_[vol.volser] = vol;
    
    // Update secondary indices
    volume_owner_index_.add(vol.owner, vol.volser);
    volume_pool_index_.add(vol.pool, vol.volser);
    for (const auto& tag : vol.tags) {
        volume_tag_index_.add(tag, vol.volser);
    }
    
    lock.unlock();
    add_audit_record("ADD_VOLUME", vol.volser, "Status: " + volume_status_to_string(vol.status));
    PerformanceMetrics::instance().increment_counter("volumes_added");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::delete_volume(const std::string& volser, bool force) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (!force && !it->second.datasets.empty()) {
        return OperationResult::err(TMSError::VOLUME_HAS_DATASETS,
            "Volume has " + std::to_string(it->second.datasets.size()) + " datasets");
    }
    
    if (it->second.status == VolumeStatus::MOUNTED) {
        return OperationResult::err(TMSError::VOLUME_MOUNTED, "Cannot delete mounted volume");
    }
    
    if (it->second.is_reserved()) {
        return OperationResult::err(TMSError::VOLUME_RESERVED, "Cannot delete reserved volume");
    }
    
    // Remove from secondary indices
    volume_owner_index_.remove(it->second.owner, volser);
    volume_pool_index_.remove(it->second.pool, volser);
    for (const auto& tag : it->second.tags) {
        volume_tag_index_.remove(tag, volser);
    }
    
    // Remove associated datasets if force delete
    if (force) {
        for (const auto& ds_name : it->second.datasets) {
            auto ds_it = datasets_.find(ds_name);
            if (ds_it != datasets_.end()) {
                dataset_owner_index_.remove(ds_it->second.owner, ds_name);
                for (const auto& tag : ds_it->second.tags) {
                    dataset_tag_index_.remove(tag, ds_name);
                }
                datasets_.erase(ds_it);
            }
        }
    }
    
    volumes_.erase(it);
    
    lock.unlock();
    add_audit_record("DELETE_VOLUME", volser, "Force: " + std::string(force ? "yes" : "no"));
    PerformanceMetrics::instance().increment_counter("volumes_deleted");
    
    return OperationResult::ok();
}

Result<TapeVolume> TMSSystem::get_volume(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return Result<TapeVolume>::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    return Result<TapeVolume>::ok(it->second);
}

OperationResult TMSSystem::update_volume(const TapeVolume& volume) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volume.volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volume.volser);
    }
    
    // Update indices if owner or pool changed
    if (it->second.owner != volume.owner) {
        volume_owner_index_.update(it->second.owner, volume.owner, volume.volser);
    }
    if (it->second.pool != volume.pool) {
        volume_pool_index_.update(it->second.pool, volume.pool, volume.volser);
    }
    
    // Update tag indices
    for (const auto& old_tag : it->second.tags) {
        if (volume.tags.find(old_tag) == volume.tags.end()) {
            volume_tag_index_.remove(old_tag, volume.volser);
        }
    }
    for (const auto& new_tag : volume.tags) {
        if (it->second.tags.find(new_tag) == it->second.tags.end()) {
            volume_tag_index_.add(new_tag, volume.volser);
        }
    }
    
    it->second = volume;
    
    lock.unlock();
    add_audit_record("UPDATE_VOLUME", volume.volser, "Updated");
    
    return OperationResult::ok();
}

std::vector<TapeVolume> TMSSystem::list_volumes(std::optional<VolumeStatus> status) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (!status.has_value() || vol.status == status.value()) {
            result.push_back(vol);
        }
    }
    return result;
}

size_t TMSSystem::get_volume_count() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return volumes_.size();
}

bool TMSSystem::volume_exists(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return volumes_.count(volser) > 0;
}

// Fast indexed lookups
std::vector<TapeVolume> TMSSystem::get_volumes_by_owner(const std::string& owner) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    auto volsers = volume_owner_index_.find(owner);
    for (const auto& volser : volsers) {
        auto it = volumes_.find(volser);
        if (it != volumes_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::vector<TapeVolume> TMSSystem::get_volumes_by_pool(const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    auto volsers = volume_pool_index_.find(pool);
    for (const auto& volser : volsers) {
        auto it = volumes_.find(volser);
        if (it != volumes_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::vector<std::string> TMSSystem::get_all_owners() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return volume_owner_index_.get_all_values();
}

std::vector<TapeVolume> TMSSystem::search_volumes(const std::string& owner,
                                                   const std::string& location,
                                                   const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    
    // Use index if only owner specified
    if (!owner.empty() && location.empty() && pool.empty()) {
        auto volsers = volume_owner_index_.find(owner);
        for (const auto& volser : volsers) {
            auto it = volumes_.find(volser);
            if (it != volumes_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }
    
    // Use index if only pool specified
    if (owner.empty() && location.empty() && !pool.empty()) {
        auto volsers = volume_pool_index_.find(pool);
        for (const auto& volser : volsers) {
            auto it = volumes_.find(volser);
            if (it != volumes_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }
    
    // Fall back to full scan for complex queries
    for (const auto& [volser, vol] : volumes_) {
        bool match = true;
        if (!owner.empty() && vol.owner != owner) match = false;
        if (!location.empty() && vol.location.find(location) == std::string::npos) match = false;
        if (!pool.empty() && vol.pool != pool) match = false;
        if (match) result.push_back(vol);
    }
    
    return result;
}

std::vector<TapeVolume> TMSSystem::search_volumes(const SearchCriteria& criteria) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    
    for (const auto& [volser, vol] : volumes_) {
        // Pattern matching (uses cached regex)
        if (!criteria.pattern.empty()) {
            if (!matches_pattern(volser, criteria.pattern, criteria.mode)) {
                continue;
            }
        }
        
        // Filter by status
        if (criteria.status.has_value() && vol.status != criteria.status.value()) {
            continue;
        }
        
        // Filter by owner
        if (criteria.owner.has_value() && vol.owner != criteria.owner.value()) {
            continue;
        }
        
        // Filter by pool
        if (criteria.pool.has_value() && vol.pool != criteria.pool.value()) {
            continue;
        }
        
        // Filter by location
        if (criteria.location.has_value() && 
            vol.location.find(criteria.location.value()) == std::string::npos) {
            continue;
        }
        
        // Filter by tag
        if (criteria.tag.has_value() && !vol.has_tag(criteria.tag.value())) {
            continue;
        }
        
        // Filter by creation date
        if (criteria.created_after.has_value() && vol.creation_date < criteria.created_after.value()) {
            continue;
        }
        if (criteria.created_before.has_value() && vol.creation_date > criteria.created_before.value()) {
            continue;
        }
        
        result.push_back(vol);
        
        // Limit results
        if (criteria.limit > 0 && result.size() >= criteria.limit) {
            break;
        }
    }
    
    return result;
}

// ============================================================================
// Dataset Management
// ============================================================================

OperationResult TMSSystem::add_dataset(const Dataset& dataset) {
    if (!validate_dataset_name(dataset.name)) {
        return OperationResult::err(TMSError::INVALID_DATASET_NAME, "Invalid dataset name: " + dataset.name);
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    if (datasets_.size() >= Configuration::instance().get_max_datasets()) {
        return OperationResult::err(TMSError::DATASET_LIMIT_REACHED, "Maximum dataset limit reached");
    }
    
    if (datasets_.count(dataset.name) > 0) {
        return OperationResult::err(TMSError::DATASET_ALREADY_EXISTS, "Dataset already exists: " + dataset.name);
    }
    
    auto vol_it = volumes_.find(dataset.volser);
    if (vol_it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + dataset.volser);
    }
    
    Dataset ds = dataset;
    if (ds.creation_date == std::chrono::system_clock::time_point{}) {
        ds.creation_date = std::chrono::system_clock::now();
    }
    if (ds.expiration_date == std::chrono::system_clock::time_point{}) {
        ds.expiration_date = ds.creation_date + std::chrono::hours(24 * 30);
    }
    
    // Add to primary storage
    datasets_[ds.name] = ds;
    
    // Update secondary indices
    dataset_owner_index_.add(ds.owner, ds.name);
    for (const auto& tag : ds.tags) {
        dataset_tag_index_.add(tag, ds.name);
    }
    
    // Update volume
    vol_it->second.datasets.push_back(ds.name);
    vol_it->second.used_bytes += ds.size_bytes;
    if (vol_it->second.status == VolumeStatus::SCRATCH) {
        vol_it->second.status = VolumeStatus::PRIVATE;
    }
    
    lock.unlock();
    add_audit_record("ADD_DATASET", ds.name, "Volume: " + ds.volser);
    PerformanceMetrics::instance().increment_counter("datasets_added");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::delete_dataset(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    // Update volume
    auto vol_it = volumes_.find(it->second.volser);
    if (vol_it != volumes_.end()) {
        auto& ds_list = vol_it->second.datasets;
        ds_list.erase(std::remove(ds_list.begin(), ds_list.end(), name), ds_list.end());
        
        if (vol_it->second.used_bytes >= it->second.size_bytes) {
            vol_it->second.used_bytes -= it->second.size_bytes;
        } else {
            vol_it->second.used_bytes = 0;
        }
        
        if (ds_list.empty() && vol_it->second.status == VolumeStatus::PRIVATE) {
            vol_it->second.status = VolumeStatus::SCRATCH;
        }
    }
    
    // Remove from secondary indices
    dataset_owner_index_.remove(it->second.owner, name);
    for (const auto& tag : it->second.tags) {
        dataset_tag_index_.remove(tag, name);
    }
    
    datasets_.erase(it);
    
    lock.unlock();
    add_audit_record("DELETE_DATASET", name, "Deleted");
    PerformanceMetrics::instance().increment_counter("datasets_deleted");
    
    return OperationResult::ok();
}

Result<Dataset> TMSSystem::get_dataset(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return Result<Dataset>::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    return Result<Dataset>::ok(it->second);
}

OperationResult TMSSystem::update_dataset(const Dataset& dataset) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(dataset.name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + dataset.name);
    }
    
    // Update owner index if changed
    if (it->second.owner != dataset.owner) {
        dataset_owner_index_.update(it->second.owner, dataset.owner, dataset.name);
    }
    
    // Update tag indices
    for (const auto& old_tag : it->second.tags) {
        if (dataset.tags.find(old_tag) == dataset.tags.end()) {
            dataset_tag_index_.remove(old_tag, dataset.name);
        }
    }
    for (const auto& new_tag : dataset.tags) {
        if (it->second.tags.find(new_tag) == it->second.tags.end()) {
            dataset_tag_index_.add(new_tag, dataset.name);
        }
    }
    
    it->second = dataset;
    
    lock.unlock();
    add_audit_record("UPDATE_DATASET", dataset.name, "Updated");
    
    return OperationResult::ok();
}

std::vector<Dataset> TMSSystem::list_datasets(std::optional<DatasetStatus> status) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Dataset> result;
    for (const auto& [name, ds] : datasets_) {
        if (!status.has_value() || ds.status == status.value()) {
            result.push_back(ds);
        }
    }
    return result;
}

std::vector<Dataset> TMSSystem::list_datasets_on_volume(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Dataset> result;
    for (const auto& [name, ds] : datasets_) {
        if (ds.volser == volser) {
            result.push_back(ds);
        }
    }
    return result;
}

std::vector<Dataset> TMSSystem::search_datasets(const SearchCriteria& criteria) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Dataset> result;
    
    for (const auto& [name, ds] : datasets_) {
        if (!criteria.pattern.empty()) {
            if (!matches_pattern(name, criteria.pattern, criteria.mode)) {
                continue;
            }
        }
        
        if (criteria.owner.has_value() && ds.owner != criteria.owner.value()) {
            continue;
        }
        
        if (criteria.tag.has_value() && !ds.has_tag(criteria.tag.value())) {
            continue;
        }
        
        if (criteria.created_after.has_value() && ds.creation_date < criteria.created_after.value()) {
            continue;
        }
        if (criteria.created_before.has_value() && ds.creation_date > criteria.created_before.value()) {
            continue;
        }
        
        result.push_back(ds);
        
        if (criteria.limit > 0 && result.size() >= criteria.limit) {
            break;
        }
    }
    
    return result;
}

size_t TMSSystem::get_dataset_count() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return datasets_.size();
}

bool TMSSystem::dataset_exists(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return datasets_.count(name) > 0;
}

std::vector<Dataset> TMSSystem::get_datasets_by_owner(const std::string& owner) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Dataset> result;
    auto names = dataset_owner_index_.find(owner);
    for (const auto& name : names) {
        auto it = datasets_.find(name);
        if (it != datasets_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

// ============================================================================
// Volume Tagging
// ============================================================================

OperationResult TMSSystem::add_volume_tag(const std::string& volser, const std::string& tag) {
    if (!validate_tag(tag)) {
        return OperationResult::err(TMSError::INVALID_TAG, "Invalid tag: " + tag);
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    it->second.tags.insert(tag);
    volume_tag_index_.add(tag, volser);
    
    lock.unlock();
    add_audit_record("ADD_TAG", volser, "Tag: " + tag);
    
    return OperationResult::ok();
}

OperationResult TMSSystem::remove_volume_tag(const std::string& volser, const std::string& tag) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    it->second.tags.erase(tag);
    volume_tag_index_.remove(tag, volser);
    
    lock.unlock();
    add_audit_record("REMOVE_TAG", volser, "Tag: " + tag);
    
    return OperationResult::ok();
}

std::vector<TapeVolume> TMSSystem::find_volumes_by_tag(const std::string& tag) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    auto volsers = volume_tag_index_.find(tag);
    for (const auto& volser : volsers) {
        auto it = volumes_.find(volser);
        if (it != volumes_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::set<std::string> TMSSystem::get_all_volume_tags() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::set<std::string> result;
    for (const auto& [volser, vol] : volumes_) {
        result.insert(vol.tags.begin(), vol.tags.end());
    }
    return result;
}

// ============================================================================
// Dataset Tagging
// ============================================================================

OperationResult TMSSystem::add_dataset_tag(const std::string& name, const std::string& tag) {
    if (!validate_tag(tag)) {
        return OperationResult::err(TMSError::INVALID_TAG, "Invalid tag: " + tag);
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    it->second.tags.insert(tag);
    dataset_tag_index_.add(tag, name);
    
    return OperationResult::ok();
}

OperationResult TMSSystem::remove_dataset_tag(const std::string& name, const std::string& tag) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    it->second.tags.erase(tag);
    dataset_tag_index_.remove(tag, name);
    
    return OperationResult::ok();
}

std::vector<Dataset> TMSSystem::find_datasets_by_tag(const std::string& tag) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Dataset> result;
    auto names = dataset_tag_index_.find(tag);
    for (const auto& name : names) {
        auto it = datasets_.find(name);
        if (it != datasets_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

std::set<std::string> TMSSystem::get_all_dataset_tags() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::set<std::string> result;
    for (const auto& [name, ds] : datasets_) {
        result.insert(ds.tags.begin(), ds.tags.end());
    }
    return result;
}

// ============================================================================
// Volume Reservation
// ============================================================================

OperationResult TMSSystem::reserve_volume(const std::string& volser, const std::string& user,
                                          std::chrono::seconds duration) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.is_reserved() && it->second.reserved_by != user) {
        return OperationResult::err(TMSError::VOLUME_RESERVED, 
            "Volume reserved by: " + it->second.reserved_by);
    }
    
    it->second.reserved_by = user;
    it->second.reservation_expires = std::chrono::system_clock::now() + duration;
    
    lock.unlock();
    add_audit_record("RESERVE_VOLUME", volser, "User: " + user + ", Duration: " + std::to_string(duration.count()) + "s");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::release_volume(const std::string& volser, const std::string& user) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (!it->second.is_reserved()) {
        return OperationResult::err(TMSError::INVALID_STATE, "Volume not reserved");
    }
    
    if (it->second.reserved_by != user) {
        return OperationResult::err(TMSError::ACCESS_DENIED, 
            "Cannot release: reserved by " + it->second.reserved_by);
    }
    
    it->second.reserved_by.clear();
    it->second.reservation_expires = std::chrono::system_clock::time_point{};
    
    lock.unlock();
    add_audit_record("RELEASE_VOLUME", volser, "User: " + user);
    
    return OperationResult::ok();
}

OperationResult TMSSystem::extend_reservation(const std::string& volser, const std::string& user,
                                              std::chrono::seconds additional_time) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (!it->second.is_reserved() || it->second.reserved_by != user) {
        return OperationResult::err(TMSError::ACCESS_DENIED, "Cannot extend: not your reservation");
    }
    
    it->second.reservation_expires += additional_time;
    
    lock.unlock();
    add_audit_record("EXTEND_RESERVATION", volser, "Additional: " + std::to_string(additional_time.count()) + "s");
    
    return OperationResult::ok();
}

std::vector<TapeVolume> TMSSystem::list_reserved_volumes() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.is_reserved()) {
            result.push_back(vol);
        }
    }
    return result;
}

size_t TMSSystem::cleanup_expired_reservations() {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    size_t count = 0;
    auto now = std::chrono::system_clock::now();
    
    for (auto& [volser, vol] : volumes_) {
        if (!vol.reserved_by.empty() && vol.reservation_expires <= now) {
            vol.reserved_by.clear();
            vol.reservation_expires = std::chrono::system_clock::time_point{};
            count++;
        }
    }
    
    if (count > 0) {
        lock.unlock();
        add_audit_record("CLEANUP_RESERVATIONS", "", "Expired: " + std::to_string(count));
    }
    
    return count;
}

// ============================================================================
// Tape Operations
// ============================================================================

OperationResult TMSSystem::mount_volume(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.status == VolumeStatus::MOUNTED) {
        return OperationResult::err(TMSError::VOLUME_MOUNTED, "Volume already mounted");
    }
    
    if (it->second.status == VolumeStatus::OFFLINE) {
        return OperationResult::err(TMSError::VOLUME_OFFLINE, "Volume is offline");
    }
    
    it->second.status = VolumeStatus::MOUNTED;
    it->second.mount_count++;
    it->second.last_used = std::chrono::system_clock::now();
    
    lock.unlock();
    add_audit_record("MOUNT_VOLUME", volser, "Mount count: " + std::to_string(it->second.mount_count));
    
    return OperationResult::ok();
}

OperationResult TMSSystem::dismount_volume(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.status != VolumeStatus::MOUNTED) {
        return OperationResult::err(TMSError::VOLUME_NOT_MOUNTED, "Volume not mounted");
    }
    
    it->second.status = it->second.datasets.empty() ? VolumeStatus::SCRATCH : VolumeStatus::PRIVATE;
    it->second.last_used = std::chrono::system_clock::now();
    
    lock.unlock();
    add_audit_record("DISMOUNT_VOLUME", volser, "");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::scratch_volume(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.status == VolumeStatus::MOUNTED) {
        return OperationResult::err(TMSError::VOLUME_MOUNTED, "Cannot scratch mounted volume");
    }
    
    // Delete all datasets on volume
    for (const auto& ds_name : it->second.datasets) {
        auto ds_it = datasets_.find(ds_name);
        if (ds_it != datasets_.end()) {
            dataset_owner_index_.remove(ds_it->second.owner, ds_name);
            for (const auto& tag : ds_it->second.tags) {
                dataset_tag_index_.remove(tag, ds_name);
            }
            datasets_.erase(ds_it);
        }
    }
    
    it->second.datasets.clear();
    it->second.used_bytes = 0;
    it->second.status = VolumeStatus::SCRATCH;
    
    lock.unlock();
    add_audit_record("SCRATCH_VOLUME", volser, "");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::migrate_dataset(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    if (it->second.status == DatasetStatus::MIGRATED) {
        return OperationResult::err(TMSError::DATASET_MIGRATED, "Dataset already migrated");
    }
    
    it->second.status = DatasetStatus::MIGRATED;
    
    lock.unlock();
    add_audit_record("MIGRATE_DATASET", name, "");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::recall_dataset(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = datasets_.find(name);
    if (it == datasets_.end()) {
        return OperationResult::err(TMSError::DATASET_NOT_FOUND, "Dataset not found: " + name);
    }
    
    if (it->second.status != DatasetStatus::MIGRATED) {
        return OperationResult::err(TMSError::INVALID_STATE, "Dataset not migrated");
    }
    
    it->second.status = DatasetStatus::RECALLED;
    it->second.last_accessed = std::chrono::system_clock::now();
    
    lock.unlock();
    add_audit_record("RECALL_DATASET", name, "");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::set_volume_offline(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.status == VolumeStatus::MOUNTED) {
        return OperationResult::err(TMSError::VOLUME_MOUNTED, "Cannot take mounted volume offline");
    }
    
    it->second.status = VolumeStatus::OFFLINE;
    
    lock.unlock();
    add_audit_record("SET_OFFLINE", volser, "");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::set_volume_online(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    if (it->second.status != VolumeStatus::OFFLINE) {
        return OperationResult::err(TMSError::INVALID_STATE, "Volume not offline");
    }
    
    it->second.status = it->second.datasets.empty() ? VolumeStatus::SCRATCH : VolumeStatus::PRIVATE;
    
    lock.unlock();
    add_audit_record("SET_ONLINE", volser, "");
    
    return OperationResult::ok();
}

// ============================================================================
// Scratch Pool Management
// ============================================================================

Result<std::string> TMSSystem::allocate_scratch_volume(const std::string& pool,
                                                        std::optional<TapeDensity> density) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    for (auto& [volser, vol] : volumes_) {
        if (vol.is_available_for_scratch()) {
            if (!pool.empty() && vol.pool != pool) continue;
            if (density.has_value() && vol.density != density.value()) continue;
            
            vol.status = VolumeStatus::PRIVATE;
            vol.last_used = std::chrono::system_clock::now();
            
            lock.unlock();
            add_audit_record("ALLOCATE_SCRATCH", volser, "Pool: " + pool);
            
            return Result<std::string>::ok(volser);
        }
    }
    
    return Result<std::string>::err(TMSError::NO_SCRATCH_AVAILABLE, "No scratch volumes available");
}

std::vector<std::string> TMSSystem::get_scratch_pool(size_t count, const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::string> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.is_available_for_scratch()) {
            if (pool.empty() || vol.pool == pool) {
                result.push_back(volser);
                if (count > 0 && result.size() >= count) break;
            }
        }
    }
    
    return result;
}

std::pair<size_t, size_t> TMSSystem::get_scratch_pool_stats(const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    size_t available = 0, total = 0;
    for (const auto& [volser, vol] : volumes_) {
        if (pool.empty() || vol.pool == pool) {
            total++;
            if (vol.is_available_for_scratch()) available++;
        }
    }
    
    return {available, total};
}

std::vector<std::string> TMSSystem::get_pool_names() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return volume_pool_index_.get_all_values();
}

PoolStatistics TMSSystem::get_pool_statistics(const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    PoolStatistics stats;
    stats.pool_name = pool;
    
    for (const auto& [volser, vol] : volumes_) {
        if (vol.pool == pool) {
            stats.total_volumes++;
            stats.total_capacity += vol.capacity_bytes;
            stats.used_capacity += vol.used_bytes;
            
            switch (vol.status) {
                case VolumeStatus::SCRATCH: stats.scratch_volumes++; break;
                case VolumeStatus::PRIVATE: stats.private_volumes++; break;
                case VolumeStatus::MOUNTED: stats.mounted_volumes++; break;
                default: break;
            }
            
            if (vol.is_reserved()) stats.reserved_volumes++;
        }
    }
    
    return stats;
}

// ============================================================================
// Batch Operations
// ============================================================================

BatchResult TMSSystem::add_volumes_batch(const std::vector<TapeVolume>& volumes) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volumes.size();
    
    for (const auto& vol : volumes) {
        auto op_result = add_volume(vol);
        if (op_result.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(vol.volser, op_result.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BatchResult TMSSystem::delete_volumes_batch(const std::vector<std::string>& volsers, bool force) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volsers.size();
    
    for (const auto& volser : volsers) {
        auto op_result = delete_volume(volser, force);
        if (op_result.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(volser, op_result.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BatchResult TMSSystem::add_datasets_batch(const std::vector<Dataset>& datasets) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = datasets.size();
    
    for (const auto& ds : datasets) {
        auto op_result = add_dataset(ds);
        if (op_result.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(ds.name, op_result.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BatchResult TMSSystem::delete_datasets_batch(const std::vector<std::string>& names) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = names.size();
    
    for (const auto& name : names) {
        auto op_result = delete_dataset(name);
        if (op_result.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(name, op_result.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

// ============================================================================
// Expiration Management
// ============================================================================

size_t TMSSystem::process_expirations(bool dry_run) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    size_t count = 0;
    auto now = std::chrono::system_clock::now();
    
    for (auto& [volser, vol] : volumes_) {
        if (vol.status != VolumeStatus::EXPIRED && vol.expiration_date < now) {
            if (!dry_run) {
                vol.status = VolumeStatus::EXPIRED;
            }
            count++;
        }
    }
    
    for (auto& [name, ds] : datasets_) {
        if (ds.status != DatasetStatus::EXPIRED && ds.expiration_date < now) {
            if (!dry_run) {
                ds.status = DatasetStatus::EXPIRED;
            }
            count++;
        }
    }
    
    if (!dry_run && count > 0) {
        lock.unlock();
        add_audit_record("PROCESS_EXPIRATIONS", "", "Count: " + std::to_string(count));
    }
    
    return count;
}

std::vector<std::string> TMSSystem::list_expired_volumes() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::string> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.status == VolumeStatus::EXPIRED) {
            result.push_back(volser);
        }
    }
    return result;
}

std::vector<std::string> TMSSystem::list_expired_datasets() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::string> result;
    for (const auto& [name, ds] : datasets_) {
        if (ds.status == DatasetStatus::EXPIRED) {
            result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> TMSSystem::list_expiring_soon(std::chrono::hours within) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::string> result;
    auto now = std::chrono::system_clock::now();
    auto threshold = now + within;
    
    for (const auto& [volser, vol] : volumes_) {
        if (vol.status != VolumeStatus::EXPIRED && 
            vol.expiration_date > now && vol.expiration_date <= threshold) {
            result.push_back("VOL:" + volser);
        }
    }
    
    for (const auto& [name, ds] : datasets_) {
        if (ds.status != DatasetStatus::EXPIRED &&
            ds.expiration_date > now && ds.expiration_date <= threshold) {
            result.push_back("DS:" + name);
        }
    }
    
    return result;
}

// ============================================================================
// Catalog Persistence
// ============================================================================

OperationResult TMSSystem::save_catalog() {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    // Save volumes
    std::ofstream vol_file(volume_catalog_path_);
    if (!vol_file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open volume catalog");
    }
    
    vol_file << "# TMS Volume Catalog v" << CATALOG_VERSION << "\n";
    vol_file << "# Generated: " << get_timestamp() << "\n";
    
    for (const auto& [volser, vol] : volumes_) {
        vol_file << "VOLUME|" << vol.volser << "|"
                 << volume_status_to_string(vol.status) << "|"
                 << density_to_string(vol.density) << "|"
                 << vol.location << "|"
                 << vol.pool << "|"
                 << vol.owner << "|"
                 << vol.mount_count << "|"
                 << (vol.write_protected ? "1" : "0") << "|"
                 << vol.capacity_bytes << "|"
                 << vol.used_bytes << "|"
                 << format_time(vol.creation_date) << "|"
                 << format_time(vol.expiration_date) << "\n";
    }
    vol_file.close();
    
    // Save datasets
    std::ofstream ds_file(dataset_catalog_path_);
    if (!ds_file.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot open dataset catalog");
    }
    
    ds_file << "# TMS Dataset Catalog v" << CATALOG_VERSION << "\n";
    ds_file << "# Generated: " << get_timestamp() << "\n";
    
    for (const auto& [name, ds] : datasets_) {
        ds_file << "DATASET|" << ds.name << "|"
                << ds.volser << "|"
                << dataset_status_to_string(ds.status) << "|"
                << ds.size_bytes << "|"
                << ds.owner << "|"
                << ds.job_name << "|"
                << ds.file_sequence << "|"
                << format_time(ds.creation_date) << "|"
                << format_time(ds.expiration_date) << "\n";
    }
    ds_file.close();
    
    TMS_LOG_DEBUG("TMSSystem", "Catalog saved: " + std::to_string(volumes_.size()) + " volumes, " +
                  std::to_string(datasets_.size()) + " datasets");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::load_catalog() {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    volumes_.clear();
    datasets_.clear();
    
    // Load volumes
    std::ifstream vol_file(volume_catalog_path_);
    if (vol_file.is_open()) {
        std::string line;
        while (std::getline(vol_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string type;
            std::getline(iss, type, '|');
            
            if (type == "VOLUME") {
                TapeVolume vol;
                std::string status_str, density_str, wp_str, cap_str, used_str, create_str, exp_str;
                std::string mount_str;
                
                std::getline(iss, vol.volser, '|');
                std::getline(iss, status_str, '|');
                std::getline(iss, density_str, '|');
                std::getline(iss, vol.location, '|');
                std::getline(iss, vol.pool, '|');
                std::getline(iss, vol.owner, '|');
                std::getline(iss, mount_str, '|');
                std::getline(iss, wp_str, '|');
                std::getline(iss, cap_str, '|');
                std::getline(iss, used_str, '|');
                std::getline(iss, create_str, '|');
                std::getline(iss, exp_str, '|');
                
                vol.status = string_to_volume_status(status_str);
                vol.density = string_to_density(density_str);
                try { vol.mount_count = std::stoi(mount_str); } catch (...) {}
                vol.write_protected = (wp_str == "1");
                try { vol.capacity_bytes = std::stoull(cap_str); } catch (...) {}
                try { vol.used_bytes = std::stoull(used_str); } catch (...) {}
                vol.creation_date = parse_time(create_str);
                vol.expiration_date = parse_time(exp_str);
                
                volumes_[vol.volser] = vol;
            }
        }
        vol_file.close();
    }
    
    // Load datasets
    std::ifstream ds_file(dataset_catalog_path_);
    if (ds_file.is_open()) {
        std::string line;
        while (std::getline(ds_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            std::string type;
            std::getline(iss, type, '|');
            
            if (type == "DATASET") {
                Dataset ds;
                std::string status_str, size_str, seq_str, create_str, exp_str;
                
                std::getline(iss, ds.name, '|');
                std::getline(iss, ds.volser, '|');
                std::getline(iss, status_str, '|');
                std::getline(iss, size_str, '|');
                std::getline(iss, ds.owner, '|');
                std::getline(iss, ds.job_name, '|');
                std::getline(iss, seq_str, '|');
                std::getline(iss, create_str, '|');
                std::getline(iss, exp_str, '|');
                
                ds.status = string_to_dataset_status(status_str);
                try { ds.size_bytes = std::stoull(size_str); } catch (...) {}
                try { ds.file_sequence = std::stoi(seq_str); } catch (...) {}
                ds.creation_date = parse_time(create_str);
                ds.expiration_date = parse_time(exp_str);
                
                datasets_[ds.name] = ds;
                
                // Update volume's dataset list
                auto vol_it = volumes_.find(ds.volser);
                if (vol_it != volumes_.end()) {
                    vol_it->second.datasets.push_back(ds.name);
                }
            }
        }
        ds_file.close();
    }
    
    // Rebuild secondary indices
    rebuild_indices();
    
    TMS_LOG_INFO("TMSSystem", "Catalog loaded: " + std::to_string(volumes_.size()) + " volumes, " +
                 std::to_string(datasets_.size()) + " datasets");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::backup_catalog(const std::string& path) const {
    std::string backup_dir = path.empty() ? data_directory_ + PATH_SEP_STR + "backups" : path;
    ensure_directory_exists(backup_dir);
    
    std::string timestamp = get_timestamp();
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    
    std::string vol_backup = backup_dir + PATH_SEP_STR + "volumes_" + timestamp + ".dat";
    std::string ds_backup = backup_dir + PATH_SEP_STR + "datasets_" + timestamp + ".dat";
    
    try {
        fs::copy_file(volume_catalog_path_, vol_backup, fs::copy_options::overwrite_existing);
        fs::copy_file(dataset_catalog_path_, ds_backup, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        return OperationResult::err(TMSError::FILE_WRITE_ERROR, "Backup failed: " + std::string(e.what()));
    }
    
    return OperationResult::ok();
}

OperationResult TMSSystem::restore_catalog(const std::string& backup_path) {
    if (!fs::exists(backup_path)) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Backup not found: " + backup_path);
    }
    
    // Implementation would copy backup files and reload
    return load_catalog();
}

// ============================================================================
// Import/Export
// ============================================================================

OperationResult TMSSystem::export_to_csv(const std::string& volumes_file, 
                                          const std::string& datasets_file) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    // Export volumes
    std::ofstream vol_out(volumes_file);
    if (!vol_out.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot create: " + volumes_file);
    }
    
    vol_out << "Volser,Status,Density,Location,Pool,Owner,MountCount,Capacity,Used,Created,Expires\n";
    for (const auto& [volser, vol] : volumes_) {
        vol_out << vol.volser << ","
                << volume_status_to_string(vol.status) << ","
                << density_to_string(vol.density) << ","
                << "\"" << vol.location << "\","
                << vol.pool << ","
                << vol.owner << ","
                << vol.mount_count << ","
                << vol.capacity_bytes << ","
                << vol.used_bytes << ","
                << format_time(vol.creation_date) << ","
                << format_time(vol.expiration_date) << "\n";
    }
    vol_out.close();
    
    // Export datasets
    std::ofstream ds_out(datasets_file);
    if (!ds_out.is_open()) {
        return OperationResult::err(TMSError::FILE_OPEN_ERROR, "Cannot create: " + datasets_file);
    }
    
    ds_out << "Name,Volser,Status,Size,Owner,JobName,FileSeq,Created,Expires\n";
    for (const auto& [name, ds] : datasets_) {
        ds_out << ds.name << ","
               << ds.volser << ","
               << dataset_status_to_string(ds.status) << ","
               << ds.size_bytes << ","
               << ds.owner << ","
               << ds.job_name << ","
               << ds.file_sequence << ","
               << format_time(ds.creation_date) << ","
               << format_time(ds.expiration_date) << "\n";
    }
    ds_out.close();
    
    return OperationResult::ok();
}

Result<BatchResult> TMSSystem::import_volumes_from_csv(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Result<BatchResult>::err(TMSError::FILE_NOT_FOUND, "Cannot open: " + file_path);
    }
    
    BatchResult result;
    std::string line;
    bool header = true;
    
    while (std::getline(file, line)) {
        if (header) { header = false; continue; }
        if (line.empty()) continue;
        
        result.total++;
        // Parse CSV and create volume... (simplified)
        TapeVolume vol;
        std::istringstream iss(line);
        std::getline(iss, vol.volser, ',');
        
        auto op = add_volume(vol);
        if (op.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(vol.volser, op.error().message);
        }
    }
    
    return Result<BatchResult>::ok(result);
}

Result<BatchResult> TMSSystem::import_datasets_from_csv(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Result<BatchResult>::err(TMSError::FILE_NOT_FOUND, "Cannot open: " + file_path);
    }
    
    BatchResult result;
    std::string line;
    bool header = true;
    
    while (std::getline(file, line)) {
        if (header) { header = false; continue; }
        if (line.empty()) continue;
        
        result.total++;
        Dataset ds;
        std::istringstream iss(line);
        std::getline(iss, ds.name, ',');
        std::getline(iss, ds.volser, ',');
        
        auto op = add_dataset(ds);
        if (op.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(ds.name, op.error().message);
        }
    }
    
    return Result<BatchResult>::ok(result);
}

// ============================================================================
// Reports
// ============================================================================

void TMSSystem::generate_volume_report(std::ostream& os, std::optional<VolumeStatus> status) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    os << "\n=== VOLUME REPORT ===\n";
    os << "Generated: " << get_timestamp() << "\n\n";
    os << std::left << std::setw(8) << "Volser" 
       << std::setw(10) << "Status"
       << std::setw(10) << "Density"
       << std::setw(15) << "Pool"
       << std::setw(10) << "Owner"
       << std::setw(12) << "Used"
       << "\n";
    os << std::string(65, '-') << "\n";
    
    for (const auto& [volser, vol] : volumes_) {
        if (status.has_value() && vol.status != status.value()) continue;
        
        os << std::left << std::setw(8) << vol.volser
           << std::setw(10) << volume_status_to_string(vol.status)
           << std::setw(10) << density_to_string(vol.density)
           << std::setw(15) << vol.pool
           << std::setw(10) << vol.owner
           << std::setw(12) << format_bytes(vol.used_bytes)
           << "\n";
    }
    
    os << "\nTotal: " << volumes_.size() << " volumes\n";
}

void TMSSystem::generate_dataset_report(std::ostream& os, const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    os << "\n=== DATASET REPORT ===\n";
    os << "Generated: " << get_timestamp() << "\n\n";
    os << std::left << std::setw(30) << "Name"
       << std::setw(8) << "Volser"
       << std::setw(10) << "Status"
       << std::setw(12) << "Size"
       << std::setw(10) << "Owner"
       << "\n";
    os << std::string(70, '-') << "\n";
    
    for (const auto& [name, ds] : datasets_) {
        if (!volser.empty() && ds.volser != volser) continue;
        
        os << std::left << std::setw(30) << ds.name
           << std::setw(8) << ds.volser
           << std::setw(10) << dataset_status_to_string(ds.status)
           << std::setw(12) << format_bytes(ds.size_bytes)
           << std::setw(10) << ds.owner
           << "\n";
    }
    
    os << "\nTotal: " << datasets_.size() << " datasets\n";
}

void TMSSystem::generate_pool_report(std::ostream& os) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    os << "\n=== POOL REPORT ===\n";
    os << "Generated: " << get_timestamp() << "\n\n";
    
    auto pools = volume_pool_index_.get_all_values();
    for (const auto& pool : pools) {
        auto stats = get_pool_statistics(pool);
        os << "Pool: " << pool << "\n";
        os << "  Total: " << stats.total_volumes << ", "
           << "Scratch: " << stats.scratch_volumes << ", "
           << "Private: " << stats.private_volumes << "\n";
        os << "  Capacity: " << format_bytes(stats.total_capacity)
           << ", Used: " << format_bytes(stats.used_capacity)
           << " (" << std::fixed << std::setprecision(1) << stats.get_utilization() << "%)\n\n";
    }
}

void TMSSystem::generate_statistics(std::ostream& os) const {
    auto stats = get_statistics();
    
    os << "\n=== SYSTEM STATISTICS ===\n";
    os << "Generated: " << get_timestamp() << "\n";
    os << "Uptime: " << stats.get_uptime() << "\n\n";
    
    os << "Volumes:\n";
    os << "  Total: " << stats.total_volumes << "\n";
    os << "  Scratch: " << stats.scratch_volumes << "\n";
    os << "  Private: " << stats.private_volumes << "\n";
    os << "  Mounted: " << stats.mounted_volumes << "\n";
    os << "  Expired: " << stats.expired_volumes << "\n";
    os << "  Reserved: " << stats.reserved_volumes << "\n\n";
    
    os << "Datasets:\n";
    os << "  Total: " << stats.total_datasets << "\n";
    os << "  Active: " << stats.active_datasets << "\n";
    os << "  Migrated: " << stats.migrated_datasets << "\n";
    os << "  Expired: " << stats.expired_datasets << "\n\n";
    
    os << "Capacity:\n";
    os << "  Total: " << format_bytes(stats.total_capacity) << "\n";
    os << "  Used: " << format_bytes(stats.used_capacity) << "\n";
    os << "  Utilization: " << std::fixed << std::setprecision(1) 
       << stats.get_utilization() << "%\n\n";
    
    os << "Cache:\n";
    os << "  Regex patterns: " << RegexCache::instance().size() << "\n";
    os << "  Audit pruned: " << audit_log_.pruned_count() << "\n";
}

void TMSSystem::generate_expiration_report(std::ostream& os) const {
    os << "\n=== EXPIRATION REPORT ===\n";
    os << "Generated: " << get_timestamp() << "\n\n";
    
    auto expired_vols = list_expired_volumes();
    auto expired_ds = list_expired_datasets();
    auto expiring = list_expiring_soon();
    
    os << "Expired Volumes: " << expired_vols.size() << "\n";
    for (const auto& v : expired_vols) {
        os << "  " << v << "\n";
    }
    
    os << "\nExpired Datasets: " << expired_ds.size() << "\n";
    for (const auto& d : expired_ds) {
        os << "  " << d << "\n";
    }
    
    os << "\nExpiring Soon (7 days): " << expiring.size() << "\n";
    for (const auto& e : expiring) {
        os << "  " << e << "\n";
    }
}

SystemStatistics TMSSystem::get_statistics() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    SystemStatistics stats;
    stats.uptime_start = start_time_;
    stats.total_volumes = volumes_.size();
    stats.total_datasets = datasets_.size();
    
    for (const auto& [volser, vol] : volumes_) {
        stats.total_capacity += vol.capacity_bytes;
        stats.used_capacity += vol.used_bytes;
        
        switch (vol.status) {
            case VolumeStatus::SCRATCH: stats.scratch_volumes++; break;
            case VolumeStatus::PRIVATE: stats.private_volumes++; break;
            case VolumeStatus::MOUNTED: stats.mounted_volumes++; break;
            case VolumeStatus::EXPIRED: stats.expired_volumes++; break;
            default: break;
        }
        
        if (vol.is_reserved()) stats.reserved_volumes++;
        
        if (!vol.pool.empty()) {
            stats.pool_counts[vol.pool]++;
        }
    }
    
    for (const auto& [name, ds] : datasets_) {
        switch (ds.status) {
            case DatasetStatus::ACTIVE: stats.active_datasets++; break;
            case DatasetStatus::MIGRATED: stats.migrated_datasets++; break;
            case DatasetStatus::EXPIRED: stats.expired_datasets++; break;
            default: break;
        }
    }
    
    return stats;
}

// ============================================================================
// Audit
// ============================================================================

void TMSSystem::add_audit_record(const std::string& operation, const std::string& target,
                                  const std::string& details, bool success) {
    audit_log_.add(operation, current_user_, target, details, success);
}

std::vector<AuditRecord> TMSSystem::get_audit_log(size_t count) const {
    return audit_log_.get_recent(count);
}

std::vector<AuditRecord> TMSSystem::search_audit_log(const std::string& operation,
                                                      const std::string& target,
                                                      size_t count) const {
    return audit_log_.search(operation, target, count);
}

OperationResult TMSSystem::export_audit_log(const std::string& path) const {
    return audit_log_.export_to_file(path);
}

void TMSSystem::clear_audit_log() {
    audit_log_.clear();
}

// ============================================================================
// Health Check
// ============================================================================

HealthCheckResult TMSSystem::perform_health_check() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    HealthCheckResult result;
    result.healthy = true;
    
    // Check scratch pool
    size_t scratch_count = 0;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.is_available_for_scratch()) scratch_count++;
    }
    
    if (scratch_count == 0) {
        result.warnings.push_back("No scratch volumes available");
    } else if (scratch_count < 10) {
        result.warnings.push_back("Low scratch pool: " + std::to_string(scratch_count) + " volumes");
    }
    
    // Check for integrity issues
    auto issues = verify_integrity();
    for (const auto& issue : issues) {
        result.errors.push_back(issue);
        result.healthy = false;
    }
    
    // Add metrics
    result.metrics["total_volumes"] = std::to_string(volumes_.size());
    result.metrics["total_datasets"] = std::to_string(datasets_.size());
    result.metrics["scratch_available"] = std::to_string(scratch_count);
    result.metrics["regex_cache_size"] = std::to_string(RegexCache::instance().size());
    result.metrics["audit_records"] = std::to_string(audit_log_.size());
    result.metrics["audit_pruned"] = std::to_string(audit_log_.pruned_count());
    
    return result;
}

std::vector<std::string> TMSSystem::verify_integrity() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::string> issues;
    
    // Check dataset-volume references
    for (const auto& [name, ds] : datasets_) {
        if (volumes_.find(ds.volser) == volumes_.end()) {
            issues.push_back("Dataset " + name + " references non-existent volume " + ds.volser);
        }
    }
    
    // Check volume dataset lists
    for (const auto& [volser, vol] : volumes_) {
        for (const auto& ds_name : vol.datasets) {
            if (datasets_.find(ds_name) == datasets_.end()) {
                issues.push_back("Volume " + volser + " references non-existent dataset " + ds_name);
            }
        }
        
        // Check capacity
        if (vol.used_bytes > vol.capacity_bytes) {
            issues.push_back("Volume " + volser + " used exceeds capacity");
        }
    }
    
    return issues;
}

void TMSSystem::update_volume_dataset_list(const std::string& volser, 
                                            const std::string& dataset_name, bool add) {
    auto it = volumes_.find(volser);
    if (it != volumes_.end()) {
        if (add) {
            it->second.datasets.push_back(dataset_name);
        } else {
            auto& ds_list = it->second.datasets;
            ds_list.erase(std::remove(ds_list.begin(), ds_list.end(), dataset_name), ds_list.end());
        }
    }
}

// ============================================================================
// v3.1.2: Bulk Tag Operations
// ============================================================================

BatchResult TMSSystem::add_tag_to_volumes(const std::vector<std::string>& volsers, const std::string& tag) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volsers.size();
    
    if (!validate_tag(tag)) {
        for (const auto& volser : volsers) {
            result.failed++;
            result.failures.emplace_back(volser, "Invalid tag: " + tag);
        }
        auto end = std::chrono::steady_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
    
    for (const auto& volser : volsers) {
        auto op = add_volume_tag(volser, tag);
        if (op.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(volser, op.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    add_audit_record("BULK_ADD_TAG", tag, "Added to " + std::to_string(result.succeeded) + " volumes");
    
    return result;
}

BatchResult TMSSystem::remove_tag_from_volumes(const std::vector<std::string>& volsers, const std::string& tag) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volsers.size();
    
    for (const auto& volser : volsers) {
        auto op = remove_volume_tag(volser, tag);
        if (op.is_success()) {
            result.succeeded++;
        } else {
            result.failed++;
            result.failures.emplace_back(volser, op.error().message);
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    add_audit_record("BULK_REMOVE_TAG", tag, "Removed from " + std::to_string(result.succeeded) + " volumes");
    
    return result;
}

// ============================================================================
// v3.1.2: Volume Cloning
// ============================================================================

Result<TapeVolume> TMSSystem::clone_volume(const std::string& source_volser, const std::string& new_volser) {
    if (!validate_volser(new_volser)) {
        return Result<TapeVolume>::err(TMSError::INVALID_VOLSER, "Invalid new volume serial: " + new_volser);
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto source_it = volumes_.find(source_volser);
    if (source_it == volumes_.end()) {
        return Result<TapeVolume>::err(TMSError::VOLUME_NOT_FOUND, "Source volume not found: " + source_volser);
    }
    
    if (volumes_.count(new_volser) > 0) {
        return Result<TapeVolume>::err(TMSError::VOLUME_ALREADY_EXISTS, "Target volume already exists: " + new_volser);
    }
    
    // Clone the volume (metadata only, not datasets)
    TapeVolume cloned = source_it->second;
    cloned.volser = new_volser;
    cloned.datasets.clear();  // New volume starts empty
    cloned.used_bytes = 0;
    cloned.mount_count = 0;
    cloned.status = VolumeStatus::SCRATCH;
    cloned.creation_date = std::chrono::system_clock::now();
    cloned.expiration_date = cloned.creation_date + std::chrono::hours(24 * 365);
    cloned.reserved_by.clear();
    cloned.reservation_expires = std::chrono::system_clock::time_point{};
    
    // Add to catalog
    volumes_[new_volser] = cloned;
    
    // Update indices
    volume_owner_index_.add(cloned.owner, new_volser);
    volume_pool_index_.add(cloned.pool, new_volser);
    for (const auto& tag : cloned.tags) {
        volume_tag_index_.add(tag, new_volser);
    }
    
    lock.unlock();
    add_audit_record("CLONE_VOLUME", new_volser, "Cloned from " + source_volser);
    PerformanceMetrics::instance().increment_counter("volumes_cloned");
    
    return Result<TapeVolume>::ok(cloned);
}

// ============================================================================
// v3.1.2: Location Tracking
// ============================================================================

OperationResult TMSSystem::update_volume_location(const std::string& volser, const std::string& new_location) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    std::string old_location = it->second.location;
    
    // Record location history entry
    LocationHistoryEntry entry;
    entry.location = old_location;
    entry.timestamp = std::chrono::system_clock::now();
    entry.moved_by = current_user_;
    entry.reason = "Location update";
    
    it->second.location_history.push_back(entry);
    
    // Keep only last MAX_LOCATION_HISTORY entries
    while (it->second.location_history.size() > MAX_LOCATION_HISTORY) {
        it->second.location_history.pop_front();
    }
    
    it->second.location = new_location;
    
    lock.unlock();
    add_audit_record("UPDATE_LOCATION", volser, "From: " + old_location + " To: " + new_location);
    
    return OperationResult::ok();
}

// ============================================================================
// v3.1.2: Enhanced Pool Management
// ============================================================================

OperationResult TMSSystem::rename_pool(const std::string& old_name, const std::string& new_name) {
    if (old_name.empty() || new_name.empty()) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Pool names cannot be empty");
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    size_t updated = 0;
    for (auto& [volser, vol] : volumes_) {
        if (vol.pool == old_name) {
            volume_pool_index_.update(old_name, new_name, volser);
            vol.pool = new_name;
            updated++;
        }
    }
    
    if (updated == 0) {
        return OperationResult::err(TMSError::POOL_NOT_FOUND, "Pool not found: " + old_name);
    }
    
    lock.unlock();
    add_audit_record("RENAME_POOL", old_name, "Renamed to " + new_name + ", " + std::to_string(updated) + " volumes updated");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::merge_pools(const std::string& source_pool, const std::string& target_pool) {
    if (source_pool.empty() || target_pool.empty()) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Pool names cannot be empty");
    }
    
    if (source_pool == target_pool) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Source and target pools must be different");
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    size_t merged = 0;
    for (auto& [volser, vol] : volumes_) {
        if (vol.pool == source_pool) {
            volume_pool_index_.update(source_pool, target_pool, volser);
            vol.pool = target_pool;
            merged++;
        }
    }
    
    if (merged == 0) {
        return OperationResult::err(TMSError::POOL_NOT_FOUND, "Source pool not found: " + source_pool);
    }
    
    lock.unlock();
    add_audit_record("MERGE_POOLS", source_pool, "Merged into " + target_pool + ", " + std::to_string(merged) + " volumes moved");
    
    return OperationResult::ok();
}



// ============================================================================
// v3.2.0: Volume Snapshots
// ============================================================================

Result<VolumeSnapshot> TMSSystem::create_volume_snapshot(const std::string& volser, 
                                                          const std::string& description) {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return Result<VolumeSnapshot>::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    auto snapshot = snapshot_manager_.create_snapshot(it->second, current_user_, description);
    
    lock.unlock();
    add_audit_record("CREATE_SNAPSHOT", volser, "Snapshot: " + snapshot.snapshot_id);
    
    return Result<VolumeSnapshot>::ok(snapshot);
}

std::vector<VolumeSnapshot> TMSSystem::get_volume_snapshots(const std::string& volser) const {
    return snapshot_manager_.get_volume_snapshots(volser);
}

std::optional<VolumeSnapshot> TMSSystem::get_snapshot(const std::string& snapshot_id) const {
    return snapshot_manager_.get_snapshot(snapshot_id);
}

OperationResult TMSSystem::delete_snapshot(const std::string& snapshot_id) {
    if (snapshot_manager_.delete_snapshot(snapshot_id)) {
        add_audit_record("DELETE_SNAPSHOT", snapshot_id, "Deleted");
        return OperationResult::ok();
    }
    return OperationResult::err(TMSError::FILE_NOT_FOUND, "Snapshot not found: " + snapshot_id);
}

OperationResult TMSSystem::restore_from_snapshot(const std::string& snapshot_id) {
    auto snap_opt = snapshot_manager_.get_snapshot(snapshot_id);
    if (!snap_opt.has_value()) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Snapshot not found: " + snapshot_id);
    }
    
    const auto& snap = snap_opt.value();
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(snap.volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + snap.volser);
    }
    
    it->second.status = snap.status_at_snapshot;
    it->second.tags = snap.tags_at_snapshot;
    it->second.notes = snap.notes_at_snapshot;
    
    lock.unlock();
    add_audit_record("RESTORE_SNAPSHOT", snap.volser, "From: " + snapshot_id);
    
    return OperationResult::ok();
}

size_t TMSSystem::get_snapshot_count() const {
    return snapshot_manager_.count();
}

// ============================================================================
// v3.2.0: Volume Health
// ============================================================================

VolumeHealthScore TMSSystem::get_volume_health(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it != volumes_.end()) {
        return it->second.health_score;
    }
    
    return VolumeHealthScore{};
}

OperationResult TMSSystem::recalculate_volume_health(const std::string& volser) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    it->second.health_score = calculate_health_score(it->second);
    it->second.last_health_check = std::chrono::system_clock::now();
    
    return OperationResult::ok();
}

BatchResult TMSSystem::recalculate_all_health() {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    result.total = volumes_.size();
    
    for (auto& [volser, vol] : volumes_) {
        vol.health_score = calculate_health_score(vol);
        vol.last_health_check = std::chrono::system_clock::now();
        result.succeeded++;
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

std::vector<TapeVolume> TMSSystem::get_unhealthy_volumes(HealthStatus min_status) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.health_score.status >= min_status) {
            result.push_back(vol);
        }
    }
    
    return result;
}

std::vector<LifecycleRecommendation> TMSSystem::get_lifecycle_recommendations() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<LifecycleRecommendation> recommendations;
    
    for (const auto& [volser, vol] : volumes_) {
        LifecycleRecommendation rec;
        rec.volser = volser;
        rec.action = LifecycleAction::NONE;
        rec.priority = 0;
        rec.auto_actionable = false;
        
        if (vol.health_score.status == HealthStatus::CRITICAL) {
            rec.action = LifecycleAction::RETIRE;
            rec.reason = "Critical health status";
            rec.priority = 10;
            recommendations.push_back(rec);
            continue;
        }
        
        if (vol.health_score.status == HealthStatus::POOR) {
            rec.action = LifecycleAction::WARN;
            rec.reason = "Poor health status - monitor closely";
            rec.priority = 7;
            recommendations.push_back(rec);
            continue;
        }
        
        if (vol.is_expired()) {
            rec.action = LifecycleAction::SCRATCH;
            rec.reason = "Volume expired";
            rec.priority = 5;
            rec.auto_actionable = true;
            recommendations.push_back(rec);
            continue;
        }
        
        if (vol.error_count > 20) {
            rec.action = LifecycleAction::MIGRATE;
            rec.reason = "High error count - migrate data";
            rec.priority = 8;
            recommendations.push_back(rec);
        }
        
        if (vol.get_usage_percent() > 95.0) {
            rec.action = LifecycleAction::ARCHIVE;
            rec.reason = "Near capacity limit";
            rec.priority = 4;
            recommendations.push_back(rec);
        }
    }
    
    std::sort(recommendations.begin(), recommendations.end(),
              [](const auto& a, const auto& b) { return a.priority > b.priority; });
    
    return recommendations;
}

// ============================================================================
// v3.2.0: Fuzzy Search
// ============================================================================

std::vector<TapeVolume> TMSSystem::fuzzy_search_volumes(const std::string& pattern, size_t threshold) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::pair<double, TapeVolume>> scored_results;
    
    for (const auto& [volser, vol] : volumes_) {
        if (fuzzy_match(volser, pattern, threshold)) {
            double score = similarity_score(volser, pattern);
            scored_results.emplace_back(score, vol);
        }
    }
    
    std::sort(scored_results.begin(), scored_results.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<TapeVolume> result;
    result.reserve(scored_results.size());
    for (const auto& [score, vol] : scored_results) {
        result.push_back(vol);
    }
    
    return result;
}

std::vector<Dataset> TMSSystem::fuzzy_search_datasets(const std::string& pattern, size_t threshold) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<std::pair<double, Dataset>> scored_results;
    
    for (const auto& [name, ds] : datasets_) {
        if (fuzzy_match(name, pattern, threshold)) {
            double score = similarity_score(name, pattern);
            scored_results.emplace_back(score, ds);
        }
    }
    
    std::sort(scored_results.begin(), scored_results.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<Dataset> result;
    result.reserve(scored_results.size());
    for (const auto& [score, ds] : scored_results) {
        result.push_back(ds);
    }
    
    return result;
}

// ============================================================================
// v3.2.0: Pool Migration
// ============================================================================

OperationResult TMSSystem::move_volume_to_pool(const std::string& volser, const std::string& target_pool) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    std::string old_pool = it->second.pool;
    volume_pool_index_.update(old_pool, target_pool, volser);
    it->second.pool = target_pool;
    
    lock.unlock();
    add_audit_record("MOVE_TO_POOL", volser, "From: " + old_pool + " To: " + target_pool);
    
    return OperationResult::ok();
}

BatchResult TMSSystem::move_volumes_to_pool(const std::vector<std::string>& volsers, const std::string& target_pool) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volsers.size();
    
    for (const auto& volser : volsers) {
        auto op = move_volume_to_pool(volser, target_pool);
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

// ============================================================================
// v3.2.0: Location History
// ============================================================================

std::vector<LocationHistoryEntry> TMSSystem::get_location_history(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it != volumes_.end()) {
        return std::vector<LocationHistoryEntry>(
            it->second.location_history.begin(), 
            it->second.location_history.end());
    }
    
    return {};
}

// ============================================================================
// v3.2.0: Health Report
// ============================================================================

void TMSSystem::generate_health_report(std::ostream& os) const {
    os << "TMS Health Report - " << get_timestamp() << "\n";
    os << std::string(70, '=') << "\n\n";
    
    auto unhealthy = get_unhealthy_volumes();
    os << "Unhealthy Volumes: " << unhealthy.size() << "\n\n";
    
    if (!unhealthy.empty()) {
        os << std::left << std::setw(8) << "Volser"
           << std::setw(10) << "Status"
           << std::setw(8) << "Score"
           << std::setw(15) << "Health"
           << "Recommendations\n";
        os << std::string(70, '-') << "\n";
        
        for (const auto& vol : unhealthy) {
            os << std::left << std::setw(8) << vol.volser
               << std::setw(10) << volume_status_to_string(vol.status)
               << std::setw(8) << std::fixed << std::setprecision(0) << vol.health_score.overall_score
               << std::setw(15) << health_status_to_string(vol.health_score.status);
            
            for (const auto& rec : vol.health_score.recommendations) {
                os << rec << "; ";
            }
            os << "\n";
        }
    }
    
    os << "\nLifecycle Recommendations:\n";
    os << std::string(50, '-') << "\n";
    
    auto recommendations = get_lifecycle_recommendations();
    for (const auto& rec : recommendations) {
        os << "  " << rec.volser << ": " 
           << lifecycle_action_to_string(rec.action) 
           << " (Priority: " << rec.priority << ") - " 
           << rec.reason << "\n";
    }
}
// ============================================================================
// v3.3.0: Encryption Metadata
// ============================================================================

OperationResult TMSSystem::set_volume_encryption(const std::string& volser, 
                                                  const EncryptionMetadata& encryption) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    it->second.encryption = encryption;
    
    lock.unlock();
    add_audit_record("SET_ENCRYPTION", volser, 
                     "Algorithm: " + encryption_algorithm_to_string(encryption.algorithm));
    
    return OperationResult::ok();
}

EncryptionMetadata TMSSystem::get_volume_encryption(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it != volumes_.end()) {
        return it->second.encryption;
    }
    
    return EncryptionMetadata{};
}

std::vector<TapeVolume> TMSSystem::get_encrypted_volumes() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.encryption.is_encrypted()) {
            result.push_back(vol);
        }
    }
    
    return result;
}

std::vector<TapeVolume> TMSSystem::get_unencrypted_volumes() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (!vol.encryption.is_encrypted()) {
            result.push_back(vol);
        }
    }
    
    return result;
}

// ============================================================================
// v3.3.0: Storage Tiering
// ============================================================================

OperationResult TMSSystem::set_volume_tier(const std::string& volser, StorageTier tier) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it == volumes_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Volume not found: " + volser);
    }
    
    StorageTier old_tier = it->second.storage_tier;
    it->second.storage_tier = tier;
    
    lock.unlock();
    add_audit_record("SET_TIER", volser, 
                     "From: " + storage_tier_to_string(old_tier) + 
                     " To: " + storage_tier_to_string(tier));
    
    return OperationResult::ok();
}

StorageTier TMSSystem::get_volume_tier(const std::string& volser) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = volumes_.find(volser);
    if (it != volumes_.end()) {
        return it->second.storage_tier;
    }
    
    return StorageTier::HOT;
}

std::vector<TapeVolume> TMSSystem::get_volumes_by_tier(StorageTier tier) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<TapeVolume> result;
    for (const auto& [volser, vol] : volumes_) {
        if (vol.storage_tier == tier) {
            result.push_back(vol);
        }
    }
    
    return result;
}

BatchResult TMSSystem::auto_tier_volumes(int days_inactive) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto threshold = now - std::chrono::hours(24 * days_inactive);
    
    for (auto& [volser, vol] : volumes_) {
        result.total++;
        
        if (vol.last_access_date < threshold && vol.storage_tier == StorageTier::HOT) {
            vol.storage_tier = StorageTier::WARM;
            result.succeeded++;
        } else if (vol.last_access_date < threshold - std::chrono::hours(24 * days_inactive) && 
                   vol.storage_tier == StorageTier::WARM) {
            vol.storage_tier = StorageTier::COLD;
            result.succeeded++;
        } else {
            result.succeeded++;
        }
    }
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

// ============================================================================
// v3.3.0: Quota Management
// ============================================================================

OperationResult TMSSystem::set_pool_quota(const std::string& pool, const Quota& quota) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    pool_quotas_[pool] = quota;
    
    lock.unlock();
    add_audit_record("SET_POOL_QUOTA", pool, 
                     "Max bytes: " + std::to_string(quota.max_bytes) + 
                     " Max volumes: " + std::to_string(quota.max_volumes));
    
    return OperationResult::ok();
}

OperationResult TMSSystem::set_owner_quota(const std::string& owner, const Quota& quota) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    owner_quotas_[owner] = quota;
    
    lock.unlock();
    add_audit_record("SET_OWNER_QUOTA", owner, 
                     "Max bytes: " + std::to_string(quota.max_bytes) + 
                     " Max volumes: " + std::to_string(quota.max_volumes));
    
    return OperationResult::ok();
}

std::optional<Quota> TMSSystem::get_pool_quota(const std::string& pool) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = pool_quotas_.find(pool);
    if (it != pool_quotas_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<Quota> TMSSystem::get_owner_quota(const std::string& owner) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = owner_quotas_.find(owner);
    if (it != owner_quotas_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool TMSSystem::check_quota_available(const std::string& pool, const std::string& owner, 
                                       uint64_t bytes) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto pool_it = pool_quotas_.find(pool);
    if (pool_it != pool_quotas_.end() && pool_it->second.enabled) {
        if (pool_it->second.max_bytes > 0 && 
            pool_it->second.used_bytes + bytes > pool_it->second.max_bytes) {
            return false;
        }
    }
    
    auto owner_it = owner_quotas_.find(owner);
    if (owner_it != owner_quotas_.end() && owner_it->second.enabled) {
        if (owner_it->second.max_bytes > 0 && 
            owner_it->second.used_bytes + bytes > owner_it->second.max_bytes) {
            return false;
        }
    }
    
    return true;
}

void TMSSystem::recalculate_quotas() {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    // Reset all quotas
    for (auto& [name, quota] : pool_quotas_) {
        quota.used_bytes = 0;
        quota.used_volumes = 0;
    }
    for (auto& [name, quota] : owner_quotas_) {
        quota.used_bytes = 0;
        quota.used_volumes = 0;
    }
    
    // Recalculate from volumes
    for (const auto& [volser, vol] : volumes_) {
        auto pool_it = pool_quotas_.find(vol.pool);
        if (pool_it != pool_quotas_.end()) {
            pool_it->second.used_bytes += vol.used_bytes;
            pool_it->second.used_volumes++;
        }
        
        auto owner_it = owner_quotas_.find(vol.owner);
        if (owner_it != owner_quotas_.end()) {
            owner_it->second.used_bytes += vol.used_bytes;
            owner_it->second.used_volumes++;
        }
    }
}

std::vector<Quota> TMSSystem::get_exceeded_quotas() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<Quota> result;
    
    for (const auto& [name, quota] : pool_quotas_) {
        if (quota.is_bytes_exceeded() || quota.is_volumes_exceeded()) {
            result.push_back(quota);
        }
    }
    
    for (const auto& [name, quota] : owner_quotas_) {
        if (quota.is_bytes_exceeded() || quota.is_volumes_exceeded()) {
            result.push_back(quota);
        }
    }
    
    return result;
}

// ============================================================================
// v3.3.0: Audit Log Export
// ============================================================================

std::string TMSSystem::export_audit_log(AuditExportFormat format) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto records = audit_log_.get_recent(MAX_AUDIT_ENTRIES);
    std::ostringstream oss;
    
    // Helper to format timestamp
    auto format_ts = [](const std::chrono::system_clock::time_point& tp) -> std::string {
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    };
    
    switch (format) {
        case AuditExportFormat::JSON: {
            oss << "[\n";
            bool first = true;
            for (const auto& entry : records) {
                if (!first) oss << ",\n";
                first = false;
                oss << "  {\"timestamp\": \"" << format_ts(entry.timestamp) << "\", "
                    << "\"operation\": \"" << entry.operation << "\", "
                    << "\"target\": \"" << entry.target << "\", "
                    << "\"user\": \"" << entry.user << "\", "
                    << "\"details\": \"" << entry.details << "\"}";
            }
            oss << "\n]";
            break;
        }
        case AuditExportFormat::CSV: {
            oss << "Timestamp,Operation,Target,User,Details\n";
            for (const auto& entry : records) {
                oss << format_ts(entry.timestamp) << ","
                    << entry.operation << ","
                    << entry.target << ","
                    << entry.user << ","
                    << "\"" << entry.details << "\"\n";
            }
            break;
        }
        case AuditExportFormat::TEXT:
        default: {
            for (const auto& entry : records) {
                oss << format_ts(entry.timestamp) << " | "
                    << std::setw(20) << entry.operation << " | "
                    << std::setw(8) << entry.target << " | "
                    << std::setw(8) << entry.user << " | "
                    << entry.details << "\n";
            }
            break;
        }
    }
    
    return oss.str();
}

void TMSSystem::export_audit_log_to_file(const std::string& filepath, 
                                          AuditExportFormat format) const {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << export_audit_log(format);
        file.close();
    }
}

// ============================================================================
// v3.3.0: Configuration Profiles
// ============================================================================

OperationResult TMSSystem::save_config_profile(const ConfigProfile& profile) {
    if (profile.name.empty() || profile.name.length() > MAX_PROFILE_NAME_LENGTH) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Invalid profile name");
    }
    
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    if (config_profiles_.size() >= MAX_PROFILES && 
        config_profiles_.find(profile.name) == config_profiles_.end()) {
        return OperationResult::err(TMSError::VOLUME_LIMIT_REACHED, "Maximum profiles reached");
    }
    
    config_profiles_[profile.name] = profile;
    
    lock.unlock();
    add_audit_record("SAVE_PROFILE", profile.name, "Saved configuration profile");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::load_config_profile(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = config_profiles_.find(name);
    if (it == config_profiles_.end()) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Profile not found: " + name);
    }
    
    // Profile found - settings can be accessed via get_config_profile()
    // Application is responsible for applying settings
    
    lock.unlock();
    add_audit_record("LOAD_PROFILE", name, "Loaded configuration profile");
    
    return OperationResult::ok();
}

OperationResult TMSSystem::delete_config_profile(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = config_profiles_.find(name);
    if (it == config_profiles_.end()) {
        return OperationResult::err(TMSError::FILE_NOT_FOUND, "Profile not found: " + name);
    }
    
    config_profiles_.erase(it);
    
    lock.unlock();
    add_audit_record("DELETE_PROFILE", name, "Deleted configuration profile");
    
    return OperationResult::ok();
}

std::vector<ConfigProfile> TMSSystem::list_config_profiles() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<ConfigProfile> result;
    result.reserve(config_profiles_.size());
    
    for (const auto& [name, profile] : config_profiles_) {
        result.push_back(profile);
    }
    
    return result;
}

std::optional<ConfigProfile> TMSSystem::get_config_profile(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    auto it = config_profiles_.find(name);
    if (it != config_profiles_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// ============================================================================
// v3.3.0: Statistics Aggregation
// ============================================================================

StatisticsAggregation TMSSystem::aggregate_volume_capacity() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<double> values;
    values.reserve(volumes_.size());
    
    for (const auto& [volser, vol] : volumes_) {
        values.push_back(static_cast<double>(vol.capacity_bytes));
    }
    
    return calculate_statistics(values);
}

StatisticsAggregation TMSSystem::aggregate_volume_usage() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<double> values;
    values.reserve(volumes_.size());
    
    for (const auto& [volser, vol] : volumes_) {
        values.push_back(vol.get_usage_percent());
    }
    
    return calculate_statistics(values);
}

StatisticsAggregation TMSSystem::aggregate_volume_health() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<double> values;
    values.reserve(volumes_.size());
    
    for (const auto& [volser, vol] : volumes_) {
        values.push_back(vol.health_score.overall_score);
    }
    
    return calculate_statistics(values);
}

StatisticsAggregation TMSSystem::aggregate_mount_counts() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<double> values;
    values.reserve(volumes_.size());
    
    for (const auto& [volser, vol] : volumes_) {
        values.push_back(static_cast<double>(vol.mount_count));
    }
    
    return calculate_statistics(values);
}

StatisticsAggregation TMSSystem::aggregate_error_counts() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    
    std::vector<double> values;
    values.reserve(volumes_.size());
    
    for (const auto& [volser, vol] : volumes_) {
        values.push_back(static_cast<double>(vol.get_total_errors()));
    }
    
    return calculate_statistics(values);
}

// ============================================================================
// v3.3.0: Parallel Batch Operations
// ============================================================================

BatchResult TMSSystem::parallel_add_volumes(const std::vector<TapeVolume>& volumes, 
                                             size_t thread_count) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volumes.size();
    
    if (volumes.empty()) {
        return result;
    }
    
    thread_count = std::min(thread_count, volumes.size());
    thread_count = std::min(thread_count, MAX_PARALLEL_OPERATIONS);
    
    std::atomic<size_t> succeeded{0};
    std::atomic<size_t> failed{0};
    std::mutex failures_mutex;
    std::vector<std::pair<std::string, std::string>> failures;
    
    auto worker = [&](size_t start_idx, size_t end_idx) {
        for (size_t i = start_idx; i < end_idx; ++i) {
            auto op = add_volume(volumes[i]);
            if (op.is_success()) {
                succeeded++;
            } else {
                failed++;
                std::lock_guard<std::mutex> lock(failures_mutex);
                failures.emplace_back(volumes[i].volser, op.error().message);
            }
        }
    };
    
    std::vector<std::thread> threads;
    size_t chunk_size = volumes.size() / thread_count;
    
    for (size_t t = 0; t < thread_count; ++t) {
        size_t start_idx = t * chunk_size;
        size_t end_idx = (t == thread_count - 1) ? volumes.size() : (t + 1) * chunk_size;
        threads.emplace_back(worker, start_idx, end_idx);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    result.succeeded = succeeded;
    result.failed = failed;
    result.failures = std::move(failures);
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BatchResult TMSSystem::parallel_delete_volumes(const std::vector<std::string>& volsers, 
                                                bool force, size_t thread_count) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volsers.size();
    
    if (volsers.empty()) {
        return result;
    }
    
    thread_count = std::min(thread_count, volsers.size());
    thread_count = std::min(thread_count, MAX_PARALLEL_OPERATIONS);
    
    std::atomic<size_t> succeeded{0};
    std::atomic<size_t> failed{0};
    std::mutex failures_mutex;
    std::vector<std::pair<std::string, std::string>> failures;
    
    auto worker = [&](size_t start_idx, size_t end_idx) {
        for (size_t i = start_idx; i < end_idx; ++i) {
            auto op = delete_volume(volsers[i], force);
            if (op.is_success()) {
                succeeded++;
            } else {
                failed++;
                std::lock_guard<std::mutex> lock(failures_mutex);
                failures.emplace_back(volsers[i], op.error().message);
            }
        }
    };
    
    std::vector<std::thread> threads;
    size_t chunk_size = volsers.size() / thread_count;
    
    for (size_t t = 0; t < thread_count; ++t) {
        size_t start_idx = t * chunk_size;
        size_t end_idx = (t == thread_count - 1) ? volsers.size() : (t + 1) * chunk_size;
        threads.emplace_back(worker, start_idx, end_idx);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    result.succeeded = succeeded;
    result.failed = failed;
    result.failures = std::move(failures);
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

BatchResult TMSSystem::parallel_update_volumes(const std::vector<TapeVolume>& volumes, 
                                                size_t thread_count) {
    auto start = std::chrono::steady_clock::now();
    BatchResult result;
    result.total = volumes.size();
    
    if (volumes.empty()) {
        return result;
    }
    
    thread_count = std::min(thread_count, volumes.size());
    thread_count = std::min(thread_count, MAX_PARALLEL_OPERATIONS);
    
    std::atomic<size_t> succeeded{0};
    std::atomic<size_t> failed{0};
    std::mutex failures_mutex;
    std::vector<std::pair<std::string, std::string>> failures;
    
    auto worker = [&](size_t start_idx, size_t end_idx) {
        for (size_t i = start_idx; i < end_idx; ++i) {
            auto op = update_volume(volumes[i]);
            if (op.is_success()) {
                succeeded++;
            } else {
                failed++;
                std::lock_guard<std::mutex> lock(failures_mutex);
                failures.emplace_back(volumes[i].volser, op.error().message);
            }
        }
    };
    
    std::vector<std::thread> threads;
    size_t chunk_size = volumes.size() / thread_count;
    
    for (size_t t = 0; t < thread_count; ++t) {
        size_t start_idx = t * chunk_size;
        size_t end_idx = (t == thread_count - 1) ? volumes.size() : (t + 1) * chunk_size;
        threads.emplace_back(worker, start_idx, end_idx);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    result.succeeded = succeeded;
    result.failed = failed;
    result.failures = std::move(failures);
    
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return result;
}

// ============================================================================
// v3.3.0: Error Recovery
// ============================================================================

void TMSSystem::set_retry_policy(const RetryPolicy& policy) {
    std::unique_lock<std::shared_mutex> lock(catalog_mutex_);
    retry_policy_ = policy;
}

RetryPolicy TMSSystem::get_retry_policy() const {
    std::shared_lock<std::shared_mutex> lock(catalog_mutex_);
    return retry_policy_;
}

RetryableResult TMSSystem::retry_operation(std::function<OperationResult()> operation) const {
    RetryableResult result;
    RetryPolicy policy = get_retry_policy();
    
    for (size_t attempt = 1; attempt <= policy.max_attempts; ++attempt) {
        result.attempts_made = static_cast<int>(attempt);
        
        auto op = operation();
        
        if (op.is_success()) {
            result.success = true;
            return result;
        }
        
        result.last_error = op.error().message;
        result.attempt_errors.push_back(op.error().message);
        
        if (attempt < policy.max_attempts) {
            int delay = calculate_retry_delay(policy, static_cast<int>(attempt));
            result.total_delay_ms += delay;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    
    result.success = false;
    return result;
}


} // namespace tms
