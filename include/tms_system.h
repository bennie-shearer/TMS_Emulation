/**
 * @file tms_system.h
 * @brief TMS Tape Management System - Core System Class
 * @version 3.3.0
 * @date 2026-01-08
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_SYSTEM_H
#define TMS_SYSTEM_H

#include "tms_types.h"
#include "tms_utils.h"
#include "error_codes.h"
#include "logger.h"

#include <map>
#include <set>
#include <shared_mutex>
#include <optional>
#include <functional>
#include <deque>

namespace tms {

// ============================================================================
// Secondary Index for Fast Lookups
// ============================================================================

template<typename KeyType = std::string>
class SecondaryIndex {
public:
    void add(const std::string& attr_value, const KeyType& key) {
        if (!attr_value.empty()) {
            index_[attr_value].insert(key);
        }
    }
    
    void remove(const std::string& attr_value, const KeyType& key) {
        auto it = index_.find(attr_value);
        if (it != index_.end()) {
            it->second.erase(key);
            if (it->second.empty()) {
                index_.erase(it);
            }
        }
    }
    
    void update(const std::string& old_value, const std::string& new_value, const KeyType& key) {
        remove(old_value, key);
        add(new_value, key);
    }
    
    std::set<KeyType> find(const std::string& attr_value) const {
        auto it = index_.find(attr_value);
        if (it != index_.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<std::string> get_all_values() const {
        std::vector<std::string> result;
        result.reserve(index_.size());
        for (const auto& [value, keys] : index_) {
            result.push_back(value);
        }
        return result;
    }
    
    void clear() { index_.clear(); }
    size_t size() const { return index_.size(); }
    
private:
    std::map<std::string, std::set<KeyType>> index_;
};

// ============================================================================
// Audit Log with Auto-Pruning
// ============================================================================

class AuditLog {
public:
    explicit AuditLog(size_t max_records = 10000) : max_records_(max_records) {}
    
    void add(const AuditRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.push_back(record);
        if (records_.size() > max_records_) {
            prune();
        }
    }
    
    void add(const std::string& operation, const std::string& user,
             const std::string& target, const std::string& details,
             bool success = true) {
        AuditRecord record;
        record.timestamp = std::chrono::system_clock::now();
        record.operation = operation;
        record.user = user;
        record.target = target;
        record.details = details;
        record.success = success;
        add(record);
    }
    
    std::vector<AuditRecord> get_recent(size_t count = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t start = records_.size() > count ? records_.size() - count : 0;
        return std::vector<AuditRecord>(records_.begin() + static_cast<long>(start), records_.end());
    }
    
    std::vector<AuditRecord> search(const std::string& operation = "",
                                     const std::string& target = "",
                                     size_t count = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<AuditRecord> result;
        for (auto it = records_.rbegin(); it != records_.rend() && result.size() < count; ++it) {
            bool match = true;
            if (!operation.empty() && it->operation.find(operation) == std::string::npos) {
                match = false;
            }
            if (!target.empty() && it->target.find(target) == std::string::npos) {
                match = false;
            }
            if (match) {
                result.push_back(*it);
            }
        }
        return result;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
        pruned_count_ = 0;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }
    
    size_t pruned_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pruned_count_;
    }
    
    OperationResult export_to_file(const std::string& path) const;
    
    void set_max_records(size_t max) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_records_ = max;
        if (records_.size() > max_records_) {
            prune();
        }
    }
    
private:
    void prune() {
        size_t to_remove = max_records_ / 5;
        if (to_remove > 0 && records_.size() > to_remove) {
            records_.erase(records_.begin(), records_.begin() + static_cast<long>(to_remove));
            pruned_count_ += to_remove;
        }
    }
    
    mutable std::mutex mutex_;
    std::vector<AuditRecord> records_;
    size_t max_records_;
    size_t pruned_count_ = 0;
};

// ============================================================================
// v3.2.0: Snapshot Manager
// ============================================================================

class SnapshotManager {
public:
    explicit SnapshotManager(size_t max_snapshots = MAX_SNAPSHOT_HISTORY)
        : max_snapshots_(max_snapshots) {}
    
    VolumeSnapshot create_snapshot(const TapeVolume& vol, 
                                   const std::string& user,
                                   const std::string& description = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        VolumeSnapshot snap;
        snap.snapshot_id = generate_snapshot_id(vol.volser);
        snap.volser = vol.volser;
        snap.created = std::chrono::system_clock::now();
        snap.created_by = user;
        snap.description = description;
        snap.status_at_snapshot = vol.status;
        snap.datasets_at_snapshot = vol.datasets;
        snap.used_bytes_at_snapshot = vol.used_bytes;
        snap.mount_count_at_snapshot = vol.mount_count;
        snap.tags_at_snapshot = vol.tags;
        snap.notes_at_snapshot = vol.notes;
        
        snapshots_[snap.snapshot_id] = snap;
        volume_snapshots_[vol.volser].push_back(snap.snapshot_id);
        
        // Limit snapshots per volume
        auto& vol_snaps = volume_snapshots_[vol.volser];
        while (vol_snaps.size() > max_snapshots_) {
            std::string oldest = vol_snaps.front();
            vol_snaps.erase(vol_snaps.begin());
            snapshots_.erase(oldest);
        }
        
        return snap;
    }
    
    std::optional<VolumeSnapshot> get_snapshot(const std::string& snapshot_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(snapshot_id);
        if (it != snapshots_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<VolumeSnapshot> get_volume_snapshots(const std::string& volser) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<VolumeSnapshot> result;
        auto it = volume_snapshots_.find(volser);
        if (it != volume_snapshots_.end()) {
            for (const auto& snap_id : it->second) {
                auto snap_it = snapshots_.find(snap_id);
                if (snap_it != snapshots_.end()) {
                    result.push_back(snap_it->second);
                }
            }
        }
        return result;
    }
    
    bool delete_snapshot(const std::string& snapshot_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(snapshot_id);
        if (it != snapshots_.end()) {
            auto& vol_snaps = volume_snapshots_[it->second.volser];
            vol_snaps.erase(std::remove(vol_snaps.begin(), vol_snaps.end(), snapshot_id), 
                           vol_snaps.end());
            snapshots_.erase(it);
            return true;
        }
        return false;
    }
    
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_.clear();
        volume_snapshots_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::map<std::string, VolumeSnapshot> snapshots_;
    std::map<std::string, std::vector<std::string>> volume_snapshots_;
    size_t max_snapshots_;
};

// ============================================================================
// TMSSystem Class
// ============================================================================

class TMSSystem {
public:
    explicit TMSSystem(const std::string& data_directory);
    ~TMSSystem();
    
    // Non-copyable
    TMSSystem(const TMSSystem&) = delete;
    TMSSystem& operator=(const TMSSystem&) = delete;
    
    // ========================================================================
    // Volume Management
    // ========================================================================
    
    OperationResult add_volume(const TapeVolume& volume);
    OperationResult delete_volume(const std::string& volser, bool force = false);
    Result<TapeVolume> get_volume(const std::string& volser) const;
    OperationResult update_volume(const TapeVolume& volume);
    std::vector<TapeVolume> list_volumes(std::optional<VolumeStatus> status = std::nullopt) const;
    std::vector<TapeVolume> search_volumes(const SearchCriteria& criteria) const;
    size_t get_volume_count() const;
    bool volume_exists(const std::string& volser) const;
    
    std::vector<TapeVolume> get_volumes_by_owner(const std::string& owner) const;
    std::vector<TapeVolume> get_volumes_by_pool(const std::string& pool) const;
    std::vector<std::string> get_all_owners() const;
    std::vector<TapeVolume> search_volumes(const std::string& owner, 
                                            const std::string& pool, 
                                            const std::string& status_str) const;
    
    // ========================================================================
    // Volume Batch Operations
    // ========================================================================
    
    BatchResult add_volumes_batch(const std::vector<TapeVolume>& volumes);
    BatchResult delete_volumes_batch(const std::vector<std::string>& volsers, bool force = false);
    
    // ========================================================================
    // Volume Tagging
    // ========================================================================
    
    OperationResult add_volume_tag(const std::string& volser, const std::string& tag);
    OperationResult remove_volume_tag(const std::string& volser, const std::string& tag);
    std::vector<TapeVolume> find_volumes_by_tag(const std::string& tag) const;
    std::set<std::string> get_all_volume_tags() const;
    
    // ========================================================================
    // v3.1.2: Bulk Tag Operations
    // ========================================================================
    
    BatchResult add_tag_to_volumes(const std::vector<std::string>& volsers, const std::string& tag);
    BatchResult remove_tag_from_volumes(const std::vector<std::string>& volsers, const std::string& tag);
    
    // ========================================================================
    // v3.1.2: Volume Cloning
    // ========================================================================
    
    Result<TapeVolume> clone_volume(const std::string& source_volser, const std::string& new_volser);
    
    // ========================================================================
    // v3.1.2: Location Tracking
    // ========================================================================
    
    OperationResult update_volume_location(const std::string& volser, const std::string& new_location);
    
    // ========================================================================
    // v3.2.0: Volume Snapshots
    // ========================================================================
    
    Result<VolumeSnapshot> create_volume_snapshot(const std::string& volser, 
                                                   const std::string& description = "");
    std::vector<VolumeSnapshot> get_volume_snapshots(const std::string& volser) const;
    std::optional<VolumeSnapshot> get_snapshot(const std::string& snapshot_id) const;
    OperationResult delete_snapshot(const std::string& snapshot_id);
    OperationResult restore_from_snapshot(const std::string& snapshot_id);
    size_t get_snapshot_count() const;
    
    // ========================================================================
    // v3.2.0: Volume Health
    // ========================================================================
    
    VolumeHealthScore get_volume_health(const std::string& volser) const;
    OperationResult recalculate_volume_health(const std::string& volser);
    BatchResult recalculate_all_health();
    std::vector<TapeVolume> get_unhealthy_volumes(HealthStatus min_status = HealthStatus::POOR) const;
    std::vector<LifecycleRecommendation> get_lifecycle_recommendations() const;
    
    // ========================================================================
    // v3.2.0: Fuzzy Search
    // ========================================================================
    
    std::vector<TapeVolume> fuzzy_search_volumes(const std::string& pattern, size_t threshold = 2) const;
    std::vector<Dataset> fuzzy_search_datasets(const std::string& pattern, size_t threshold = 2) const;
    
    // ========================================================================
    // v3.2.0: Volume Migration Between Pools
    // ========================================================================
    
    OperationResult move_volume_to_pool(const std::string& volser, const std::string& target_pool);
    BatchResult move_volumes_to_pool(const std::vector<std::string>& volsers, const std::string& target_pool);
    
    // ========================================================================
    // v3.2.0: Location History
    // ========================================================================
    
    std::vector<LocationHistoryEntry> get_location_history(const std::string& volser) const;
    
    // ========================================================================
    // Volume Reservation
    // ========================================================================
    
    OperationResult reserve_volume(const std::string& volser, const std::string& user,
                                   std::chrono::seconds duration = std::chrono::seconds(3600));
    OperationResult release_volume(const std::string& volser, const std::string& user);
    OperationResult extend_reservation(const std::string& volser, const std::string& user,
                                       std::chrono::seconds additional_time);
    std::vector<TapeVolume> list_reserved_volumes() const;
    size_t cleanup_expired_reservations();
    
    // ========================================================================
    // Dataset Management
    // ========================================================================
    
    OperationResult add_dataset(const Dataset& dataset);
    OperationResult delete_dataset(const std::string& name);
    Result<Dataset> get_dataset(const std::string& name) const;
    OperationResult update_dataset(const Dataset& dataset);
    std::vector<Dataset> list_datasets(std::optional<DatasetStatus> status = std::nullopt) const;
    std::vector<Dataset> list_datasets_on_volume(const std::string& volser) const;
    std::vector<Dataset> search_datasets(const SearchCriteria& criteria) const;
    size_t get_dataset_count() const;
    bool dataset_exists(const std::string& name) const;
    std::vector<Dataset> get_datasets_by_owner(const std::string& owner) const;
    
    // ========================================================================
    // Dataset Batch Operations
    // ========================================================================
    
    BatchResult add_datasets_batch(const std::vector<Dataset>& datasets);
    BatchResult delete_datasets_batch(const std::vector<std::string>& names);
    
    // ========================================================================
    // Dataset Tagging
    // ========================================================================
    
    OperationResult add_dataset_tag(const std::string& name, const std::string& tag);
    OperationResult remove_dataset_tag(const std::string& name, const std::string& tag);
    std::vector<Dataset> find_datasets_by_tag(const std::string& tag) const;
    std::set<std::string> get_all_dataset_tags() const;
    
    // ========================================================================
    // Tape Operations
    // ========================================================================
    
    OperationResult mount_volume(const std::string& volser);
    OperationResult dismount_volume(const std::string& volser);
    OperationResult scratch_volume(const std::string& volser);
    OperationResult migrate_dataset(const std::string& name);
    OperationResult recall_dataset(const std::string& name);
    OperationResult set_volume_offline(const std::string& volser);
    OperationResult set_volume_online(const std::string& volser);
    
    // ========================================================================
    // Scratch Pool Management
    // ========================================================================
    
    Result<std::string> allocate_scratch_volume(const std::string& pool = "",
                                                std::optional<TapeDensity> density = std::nullopt);
    std::vector<std::string> get_scratch_pool(size_t count = 0, const std::string& pool = "") const;
    std::pair<size_t, size_t> get_scratch_pool_stats(const std::string& pool = "") const;
    std::vector<std::string> get_pool_names() const;
    PoolStatistics get_pool_statistics(const std::string& pool) const;
    
    // ========================================================================
    // v3.1.2: Enhanced Pool Management
    // ========================================================================
    
    OperationResult rename_pool(const std::string& old_name, const std::string& new_name);
    OperationResult merge_pools(const std::string& source_pool, const std::string& target_pool);
    
    // ========================================================================
    // Expiration Management
    // ========================================================================
    
    size_t process_expirations(bool dry_run = false);
    std::vector<std::string> list_expired_volumes() const;
    std::vector<std::string> list_expired_datasets() const;
    std::vector<std::string> list_expiring_soon(std::chrono::hours within = std::chrono::hours(24 * 7)) const;
    
    // ========================================================================
    // Catalog Persistence
    // ========================================================================
    
    OperationResult save_catalog();
    OperationResult load_catalog();
    OperationResult backup_catalog(const std::string& path = "") const;
    OperationResult restore_catalog(const std::string& backup_path);
    
    // ========================================================================
    // Import/Export
    // ========================================================================
    
    OperationResult export_to_csv(const std::string& volumes_file, const std::string& datasets_file) const;
    Result<BatchResult> import_volumes_from_csv(const std::string& file_path);
    Result<BatchResult> import_datasets_from_csv(const std::string& file_path);
    
    // ========================================================================
    // Reports
    // ========================================================================
    
    void generate_volume_report(std::ostream& os, std::optional<VolumeStatus> status = std::nullopt) const;
    void generate_dataset_report(std::ostream& os, const std::string& volser = "") const;
    void generate_pool_report(std::ostream& os) const;
    void generate_statistics(std::ostream& os) const;
    void generate_expiration_report(std::ostream& os) const;
    void generate_health_report(std::ostream& os) const;  // v3.2.0
    
    // ========================================================================
    // v3.3.0: Encryption Metadata
    // ========================================================================
    
    OperationResult set_volume_encryption(const std::string& volser, 
                                          const EncryptionMetadata& encryption);
    EncryptionMetadata get_volume_encryption(const std::string& volser) const;
    std::vector<TapeVolume> get_encrypted_volumes() const;
    std::vector<TapeVolume> get_unencrypted_volumes() const;
    
    // ========================================================================
    // v3.3.0: Storage Tiering
    // ========================================================================
    
    OperationResult set_volume_tier(const std::string& volser, StorageTier tier);
    StorageTier get_volume_tier(const std::string& volser) const;
    std::vector<TapeVolume> get_volumes_by_tier(StorageTier tier) const;
    BatchResult auto_tier_volumes(int days_inactive = 30);
    
    // ========================================================================
    // v3.3.0: Quota Management
    // ========================================================================
    
    OperationResult set_pool_quota(const std::string& pool, const Quota& quota);
    OperationResult set_owner_quota(const std::string& owner, const Quota& quota);
    std::optional<Quota> get_pool_quota(const std::string& pool) const;
    std::optional<Quota> get_owner_quota(const std::string& owner) const;
    bool check_quota_available(const std::string& pool, const std::string& owner, 
                               uint64_t bytes) const;
    void recalculate_quotas();
    std::vector<Quota> get_exceeded_quotas() const;
    
    // ========================================================================
    // v3.3.0: Audit Log Export
    // ========================================================================
    
    std::string export_audit_log(AuditExportFormat format) const;
    void export_audit_log_to_file(const std::string& filepath, AuditExportFormat format) const;
    
    // ========================================================================
    // v3.3.0: Configuration Profiles
    // ========================================================================
    
    OperationResult save_config_profile(const ConfigProfile& profile);
    OperationResult load_config_profile(const std::string& name);
    OperationResult delete_config_profile(const std::string& name);
    std::vector<ConfigProfile> list_config_profiles() const;
    std::optional<ConfigProfile> get_config_profile(const std::string& name) const;
    
    // ========================================================================
    // v3.3.0: Statistics Aggregation
    // ========================================================================
    
    StatisticsAggregation aggregate_volume_capacity() const;
    StatisticsAggregation aggregate_volume_usage() const;
    StatisticsAggregation aggregate_volume_health() const;
    StatisticsAggregation aggregate_mount_counts() const;
    StatisticsAggregation aggregate_error_counts() const;
    
    // ========================================================================
    // v3.3.0: Parallel Batch Operations
    // ========================================================================
    
    BatchResult parallel_add_volumes(const std::vector<TapeVolume>& volumes, 
                                      size_t thread_count = 4);
    BatchResult parallel_delete_volumes(const std::vector<std::string>& volsers, 
                                         bool force = false, size_t thread_count = 4);
    BatchResult parallel_update_volumes(const std::vector<TapeVolume>& volumes, 
                                         size_t thread_count = 4);
    
    // ========================================================================
    // v3.3.0: Error Recovery
    // ========================================================================
    
    void set_retry_policy(const RetryPolicy& policy);
    RetryPolicy get_retry_policy() const;
    RetryableResult retry_operation(std::function<OperationResult()> operation) const;
    SystemStatistics get_statistics() const;
    
    // ========================================================================
    // Audit
    // ========================================================================
    
    std::vector<AuditRecord> get_audit_log(size_t count = 100) const;
    std::vector<AuditRecord> search_audit_log(const std::string& operation = "",
                                               const std::string& target = "",
                                               size_t count = 100) const;
    OperationResult export_audit_log(const std::string& path) const;
    void clear_audit_log();
    size_t get_audit_pruned_count() const { return audit_log_.pruned_count(); }
    
    // ========================================================================
    // Health Check
    // ========================================================================
    
    HealthCheckResult perform_health_check() const;
    std::vector<std::string> verify_integrity() const;
    
    // ========================================================================
    // Utility
    // ========================================================================
    
    const std::string& get_data_directory() const { return data_directory_; }
    static bool ensure_directory_exists(const std::string& path);
    void set_current_user(const std::string& user) { current_user_ = user; }
    std::string get_current_user() const { return current_user_; }
    size_t get_regex_cache_size() const { return RegexCache::instance().size(); }
    void clear_regex_cache() { RegexCache::instance().clear(); }
    
private:
    void add_audit_record(const std::string& operation, const std::string& target,
                          const std::string& details, bool success = true);
    void update_volume_dataset_list(const std::string& volser, const std::string& dataset_name, bool add);
    void rebuild_indices();
    
    std::string data_directory_;
    std::string volume_catalog_path_;
    std::string dataset_catalog_path_;
    std::string current_user_ = "SYSTEM";
    
    mutable std::shared_mutex catalog_mutex_;
    std::map<std::string, TapeVolume> volumes_;
    std::map<std::string, Dataset> datasets_;
    
    SecondaryIndex<std::string> volume_owner_index_;
    SecondaryIndex<std::string> volume_pool_index_;
    SecondaryIndex<std::string> volume_tag_index_;
    SecondaryIndex<std::string> dataset_owner_index_;
    SecondaryIndex<std::string> dataset_tag_index_;
    
    AuditLog audit_log_{10000};
    SnapshotManager snapshot_manager_;
    
    // v3.3.0 members
    std::map<std::string, Quota> pool_quotas_;
    std::map<std::string, Quota> owner_quotas_;
    std::map<std::string, ConfigProfile> config_profiles_;
    RetryPolicy retry_policy_;
    
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace tms

#endif // TMS_SYSTEM_H
