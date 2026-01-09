# TMS Emulation - API Reference
Version  3.3.0

## Table of Contents

1. [Overview](#overview)
2. [TMSSystem Class](#tmssystem-class)
3. [Data Structures](#data-structures)
4. [Enumerations](#enumerations)
5. [Utility Functions](#utility-functions)
6. [Error Handling](#error-handling)
7. [Configuration](#configuration)
8. [Logging](#logging)

---

## Overview

The TMS API is organized around the central `TMSSystem` class, which provides
all tape management functionality. Supporting data structures, enumerations,
and utility functions complete the API.

**Namespace:** `tms`

**Headers:**
- `tms_tape_mgmt.h` - Core functionality
- `error_codes.h` - Error types and result classes
- `logger.h` - Logging framework
- `configuration.h` - Configuration management
- `tms_version.h` - Version information

---

## TMSSystem Class

The main class for all tape management operations.

### Constructor

```cpp
explicit TMSSystem(const std::string& data_directory = "tms_data");
```

Creates a new TMS system instance with the specified data directory.

**Parameters:**
- `data_directory` - Path where catalog files will be stored

**Example:**
```cpp
TMSSystem system("my_tape_data");
```

---

### Volume Management

#### add_volume

```cpp
OperationResult add_volume(const TapeVolume& volume);
```

Adds a new tape volume to the catalog.

**Parameters:**
- `volume` - TapeVolume structure with volume details

**Returns:** OperationResult indicating success or failure

**Example:**
```cpp
TapeVolume vol;
vol.volser = "VOL001";
vol.status = VolumeStatus::SCRATCH;
vol.density = TapeDensity::DENSITY_LTO3;
auto result = system.add_volume(vol);
```

#### delete_volume

```cpp
OperationResult delete_volume(const std::string& volser, bool force = false);
```

Removes a volume from the catalog.

**Parameters:**
- `volser` - Volume serial number
- `force` - If true, delete even if datasets exist

#### get_volume

```cpp
Result<TapeVolume> get_volume(const std::string& volser) const;
```

Retrieves volume information.

**Returns:** Result containing TapeVolume or error

#### update_volume

```cpp
OperationResult update_volume(const TapeVolume& volume);
```

Updates an existing volume's attributes.

#### list_volumes

```cpp
std::vector<TapeVolume> list_volumes(
    std::optional<VolumeStatus> status = std::nullopt) const;
```

Lists all volumes, optionally filtered by status.

#### search_volumes

```cpp
std::vector<TapeVolume> search_volumes(
    const std::string& owner = "",
    const std::string& location = "",
    const std::string& pool = "") const;

std::vector<TapeVolume> search_volumes(const SearchCriteria& criteria) const;
```

Searches volumes by various criteria.

#### get_volume_count

```cpp
size_t get_volume_count() const;
```

Returns the total number of volumes.

#### volume_exists

```cpp
bool volume_exists(const std::string& volser) const;
```

Checks if a volume exists.

---

### Volume Batch Operations

#### add_volumes_batch

```cpp
BatchResult add_volumes_batch(const std::vector<TapeVolume>& volumes);
```

Adds multiple volumes in a single operation.

**Returns:** BatchResult with success/failure counts

#### delete_volumes_batch

```cpp
BatchResult delete_volumes_batch(
    const std::vector<std::string>& volsers, 
    bool force = false);
```

Deletes multiple volumes.

---

### Volume Tagging

#### add_volume_tag

```cpp
OperationResult add_volume_tag(
    const std::string& volser, 
    const std::string& tag);
```

Adds a tag to a volume.

#### remove_volume_tag

```cpp
OperationResult remove_volume_tag(
    const std::string& volser, 
    const std::string& tag);
```

Removes a tag from a volume.

#### find_volumes_by_tag

```cpp
std::vector<TapeVolume> find_volumes_by_tag(const std::string& tag) const;
```

Finds all volumes with a specific tag.

#### get_all_volume_tags

```cpp
std::set<std::string> get_all_volume_tags() const;
```

Returns all unique tags across volumes.

---

### Volume Reservation

#### reserve_volume

```cpp
OperationResult reserve_volume(
    const std::string& volser,
    const std::string& user,
    std::chrono::seconds duration = std::chrono::seconds(3600));
```

Reserves a volume for exclusive use.

#### release_volume

```cpp
OperationResult release_volume(
    const std::string& volser, 
    const std::string& user);
```

Releases a reserved volume.

#### extend_reservation

```cpp
OperationResult extend_reservation(
    const std::string& volser,
    const std::string& user,
    std::chrono::seconds additional_time);
```

Extends an existing reservation.

#### list_reserved_volumes

```cpp
std::vector<TapeVolume> list_reserved_volumes() const;
```

Lists all currently reserved volumes.

---

### Dataset Management

#### add_dataset

```cpp
OperationResult add_dataset(const Dataset& dataset);
```

Adds a new dataset to the catalog.

#### delete_dataset

```cpp
OperationResult delete_dataset(const std::string& name);
```

Removes a dataset from the catalog.

#### get_dataset

```cpp
Result<Dataset> get_dataset(const std::string& name) const;
```

Retrieves dataset information.

#### update_dataset

```cpp
OperationResult update_dataset(const Dataset& dataset);
```

Updates dataset attributes.

#### list_datasets

```cpp
std::vector<Dataset> list_datasets(
    std::optional<DatasetStatus> status = std::nullopt) const;
```

Lists datasets, optionally filtered by status.

#### list_datasets_on_volume

```cpp
std::vector<Dataset> list_datasets_on_volume(const std::string& volser) const;
```

Lists all datasets on a specific volume.

---

### Tape Operations

#### mount_volume

```cpp
OperationResult mount_volume(const std::string& volser);
```

Mounts a volume (marks as in-use).

#### dismount_volume

```cpp
OperationResult dismount_volume(const std::string& volser);
```

Dismounts a volume.

#### scratch_volume

```cpp
OperationResult scratch_volume(const std::string& volser);
```

Returns a volume to scratch status, removing all datasets.

#### migrate_dataset

```cpp
OperationResult migrate_dataset(const std::string& name);
```

Marks a dataset as migrated.

#### recall_dataset

```cpp
OperationResult recall_dataset(const std::string& name);
```

Recalls a migrated dataset.

---

### Scratch Pool Management

#### allocate_scratch_volume

```cpp
Result<std::string> allocate_scratch_volume(
    const std::string& pool = "",
    std::optional<TapeDensity> density = std::nullopt);
```

Allocates a scratch volume from the pool.

**Returns:** Result containing the allocated VOLSER

#### get_scratch_pool

```cpp
std::vector<std::string> get_scratch_pool(
    size_t count = 0, 
    const std::string& pool = "") const;
```

Gets list of available scratch volumes.

#### get_scratch_pool_stats

```cpp
std::pair<size_t, size_t> get_scratch_pool_stats(
    const std::string& pool = "") const;
```

Returns (available, total) counts for scratch pool.

#### get_pool_names

```cpp
std::vector<std::string> get_pool_names() const;
```

Returns all defined pool names.

#### get_pool_statistics

```cpp
PoolStatistics get_pool_statistics(const std::string& pool) const;
```

Gets detailed statistics for a pool.

---

### Expiration Management

#### process_expirations

```cpp
size_t process_expirations(bool dry_run = false);
```

Processes expired volumes and datasets.

**Parameters:**
- `dry_run` - If true, only counts without changing status

**Returns:** Number of items processed

#### list_expired_volumes

```cpp
std::vector<std::string> list_expired_volumes() const;
```

Lists all expired volumes.

#### list_expired_datasets

```cpp
std::vector<std::string> list_expired_datasets() const;
```

Lists all expired datasets.

#### list_expiring_soon

```cpp
std::vector<std::string> list_expiring_soon(
    std::chrono::hours within = std::chrono::hours(24 * 7)) const;
```

Lists items expiring within the specified time.

---

### Catalog Persistence

#### save_catalog

```cpp
OperationResult save_catalog();
```

Saves the catalog to disk.

#### load_catalog

```cpp
OperationResult load_catalog();
```

Loads the catalog from disk.

#### backup_catalog

```cpp
OperationResult backup_catalog(const std::string& path = "") const;
```

Creates a backup of the catalog.

---

### Reports and Statistics

#### generate_volume_report

```cpp
void generate_volume_report(
    std::ostream& os,
    std::optional<VolumeStatus> status = std::nullopt) const;
```

Generates a volume report to the output stream.

#### generate_dataset_report

```cpp
void generate_dataset_report(
    std::ostream& os,
    const std::string& volser = "") const;
```

Generates a dataset report.

#### generate_statistics

```cpp
void generate_statistics(std::ostream& os) const;
```

Generates system statistics.

#### get_statistics

```cpp
SystemStatistics get_statistics() const;
```

Returns statistics as a structure.

#### perform_health_check

```cpp
HealthCheckResult perform_health_check() const;
```

Performs a system health check.

#### verify_integrity

```cpp
std::vector<std::string> verify_integrity() const;
```

Checks catalog integrity, returns list of issues.

---

### Audit

#### get_audit_log

```cpp
std::vector<AuditRecord> get_audit_log(size_t count = 100) const;
```

Gets recent audit records.

#### export_audit_log

```cpp
OperationResult export_audit_log(const std::string& path) const;
```

Exports audit log to a file.

---

## Data Structures

### TapeVolume

```cpp
struct TapeVolume {
    std::string volser;                              // Volume serial (1-6 chars)
    VolumeStatus status;                             // Current status
    TapeDensity density;                             // Tape density/type
    std::string location;                            // Physical location
    std::string pool;                                // Pool assignment
    std::string owner;                               // Owner ID
    std::chrono::system_clock::time_point creation_date;
    std::chrono::system_clock::time_point expiration_date;
    std::chrono::system_clock::time_point last_used;
    int mount_count;                                 // Number of mounts
    bool write_protected;
    uint64_t capacity_bytes;                         // Total capacity
    uint64_t used_bytes;                             // Used space
    int error_count;                                 // Error counter
    std::vector<std::string> datasets;               // Dataset names
    std::set<std::string> tags;                      // Custom tags
    std::string notes;                               // Free-form notes
    std::string reserved_by;                         // Reservation user
    std::chrono::system_clock::time_point reservation_expires;
    
    // Methods
    double get_usage_percent() const;
    uint64_t get_free_bytes() const;
    bool is_expired() const;
    bool is_reserved() const;
    bool has_tag(const std::string& tag) const;
    bool is_available_for_scratch() const;
};
```

### Dataset

```cpp
struct Dataset {
    std::string name;                                // Dataset name (1-44 chars)
    std::string volser;                              // Volume reference
    DatasetStatus status;                            // Current status
    size_t size_bytes;                               // Dataset size
    std::string owner;                               // Owner ID
    std::string job_name;                            // Creating job
    int file_sequence;                               // File sequence number
    int generation;                                  // GDG generation
    int version;                                     // GDG version
    std::string record_format;                       // RECFM
    size_t block_size;                               // BLKSIZE
    size_t record_length;                            // LRECL
    std::chrono::system_clock::time_point creation_date;
    std::chrono::system_clock::time_point expiration_date;
    std::chrono::system_clock::time_point last_accessed;
    std::set<std::string> tags;
    std::string notes;
    
    // Methods
    bool is_gdg() const;
    bool is_expired() const;
    bool has_tag(const std::string& tag) const;
    std::string get_full_name() const;
};
```

### BatchResult

```cpp
struct BatchResult {
    size_t total;                                    // Total attempted
    size_t succeeded;                                // Successful operations
    size_t failed;                                   // Failed operations
    size_t skipped;                                  // Skipped operations
    std::vector<std::pair<std::string, std::string>> failures;
    std::chrono::milliseconds duration;
    
    bool all_succeeded() const;
    bool any_succeeded() const;
    double success_rate() const;
};
```

### SearchCriteria

```cpp
struct SearchCriteria {
    std::string pattern;                             // Search pattern
    SearchMode mode;                                 // Match mode
    std::optional<VolumeStatus> status;
    std::optional<std::string> owner;
    std::optional<std::string> pool;
    std::optional<std::string> location;
    std::optional<std::string> tag;
    std::optional<std::chrono::system_clock::time_point> created_after;
    std::optional<std::chrono::system_clock::time_point> created_before;
    size_t limit;                                    // Max results (0 = unlimited)
};
```

---

## Enumerations

### VolumeStatus

```cpp
enum class VolumeStatus {
    SCRATCH,        // Available for allocation
    PRIVATE,        // Contains data
    ARCHIVED,       // Archived to vault
    EXPIRED,        // Past expiration date
    MOUNTED,        // Currently mounted
    OFFLINE,        // Taken offline
    RESERVED,       // Reserved for exclusive use
    VOLUME_ERROR    // Error condition
};
```

### DatasetStatus

```cpp
enum class DatasetStatus {
    ACTIVE,         // In use
    MIGRATED,       // Migrated to ML2
    EXPIRED,        // Past expiration
    DELETED,        // Marked for deletion
    RECALLED,       // Recalled from migration
    PENDING         // Pending operation
};
```

### TapeDensity

```cpp
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
```

### SearchMode

```cpp
enum class SearchMode {
    EXACT,          // Exact match
    PREFIX,         // Starts with
    SUFFIX,         // Ends with
    CONTAINS,       // Contains substring
    WILDCARD,       // Wildcard (* and ?)
    REGEX           // Regular expression
};
```

---

## Utility Functions

### Formatting

```cpp
std::string format_time(const std::chrono::system_clock::time_point& tp);
std::string format_bytes(uint64_t bytes);
std::string format_duration(std::chrono::seconds duration);
std::string get_timestamp();
```

### Conversions

```cpp
std::string volume_status_to_string(VolumeStatus status);
VolumeStatus string_to_volume_status(const std::string& str);

std::string dataset_status_to_string(DatasetStatus status);
DatasetStatus string_to_dataset_status(const std::string& str);

std::string density_to_string(TapeDensity density);
TapeDensity string_to_density(const std::string& str);

uint64_t get_density_capacity(TapeDensity density);
```

### Validation

```cpp
bool validate_volser(const std::string& volser);
bool validate_dataset_name(const std::string& name);
bool validate_tag(const std::string& tag);
bool validate_owner(const std::string& owner);
```

### String Utilities

```cpp
std::string to_upper(std::string str);
std::string trim(const std::string& str);
bool matches_pattern(const std::string& text, 
                     const std::string& pattern, 
                     SearchMode mode);
```

---

## Error Handling

### TMSError Enumeration

Error codes are organized by category:

- **100-199**: Volume errors
- **200-299**: Dataset errors
- **300-399**: Operation errors
- **400-499**: I/O errors
- **500-599**: System errors
- **600-699**: Validation errors
- **700-799**: Security errors
- **800-899**: Audit errors
- **900-999**: Scratch pool errors

### Result<T> Class

```cpp
template<typename T>
class Result {
    static Result ok(const T& value);
    static Result err(TMSError code, const std::string& message);
    
    bool is_success() const;
    bool is_error() const;
    const T& value() const;
    const ErrorInfo& error() const;
};
```

### OperationResult Class

```cpp
class OperationResult {
    static OperationResult ok();
    static OperationResult err(TMSError code, const std::string& message);
    
    bool is_success() const;
    bool is_error() const;
    const ErrorInfo& error() const;
};
```

---

## Configuration

The Configuration class manages system settings:

```cpp
Configuration& config = Configuration::instance();

// Load/save
config.load_from_file("tms.conf");
config.save_to_file("tms.conf");

// Access settings
std::string data_dir = config.get_data_directory();
size_t max_vols = config.get_max_volumes();
std::string log_level = config.get_log_level();
```

---

## Logging

The Logger class provides thread-safe logging:

```cpp
Logger& log = Logger::instance();

log.set_level(Logger::Level::INFO);
log.enable_console(true);
log.set_log_file("tms.log");

TMS_LOG_INFO("Component", "Message");
TMS_LOG_ERROR("Component", "Error message");
```

---

*For more information, see the source code documentation and examples.*


## v3.2.0 API Additions

### Volume Snapshots

```cpp
// Create a point-in-time snapshot
Result<VolumeSnapshot> create_volume_snapshot(const std::string& volser, 
                                               const std::string& description = "");

// Get all snapshots for a volume
std::vector<VolumeSnapshot> get_volume_snapshots(const std::string& volser) const;

// Get a specific snapshot
std::optional<VolumeSnapshot> get_snapshot(const std::string& snapshot_id) const;

// Delete a snapshot
OperationResult delete_snapshot(const std::string& snapshot_id);

// Restore volume metadata from snapshot
OperationResult restore_from_snapshot(const std::string& snapshot_id);

// Get total snapshot count
size_t get_snapshot_count() const;
```

### Volume Health

```cpp
// Get health score for a volume
VolumeHealthScore get_volume_health(const std::string& volser) const;

// Recalculate health for a single volume
OperationResult recalculate_volume_health(const std::string& volser);

// Recalculate health for all volumes
BatchResult recalculate_all_health();

// Get volumes with poor health
std::vector<TapeVolume> get_unhealthy_volumes(HealthStatus min_status = HealthStatus::POOR) const;

// Get automated lifecycle recommendations
std::vector<LifecycleRecommendation> get_lifecycle_recommendations() const;
```

### Fuzzy Search

```cpp
// Fuzzy search volumes by volser
std::vector<TapeVolume> fuzzy_search_volumes(const std::string& pattern, 
                                              size_t threshold = 2) const;

// Fuzzy search datasets by name
std::vector<Dataset> fuzzy_search_datasets(const std::string& pattern, 
                                            size_t threshold = 2) const;
```

### Pool Migration

```cpp
// Move a single volume to a different pool
OperationResult move_volume_to_pool(const std::string& volser, 
                                     const std::string& target_pool);

// Move multiple volumes to a pool
BatchResult move_volumes_to_pool(const std::vector<std::string>& volsers, 
                                  const std::string& target_pool);
```

### Location History

```cpp
// Get location history for a volume
std::vector<LocationHistoryEntry> get_location_history(const std::string& volser) const;
```

### Health Reports

```cpp
// Generate health report
void generate_health_report(std::ostream& os) const;
```

### New Data Structures

#### VolumeHealthScore
```cpp
struct VolumeHealthScore {
    double overall_score;           // Overall health (0-100)
    HealthStatus status;            // Categorized status
    double error_rate_score;        // Based on error count
    double age_score;               // Based on volume age
    double usage_score;             // Based on mount count
    double capacity_score;          // Based on capacity usage
    std::chrono::system_clock::time_point last_calculated;
    std::vector<std::string> recommendations;
    
    bool is_healthy() const;
    static HealthStatus score_to_status(double score);
};
```

#### VolumeSnapshot
```cpp
struct VolumeSnapshot {
    std::string snapshot_id;
    std::string volser;
    std::chrono::system_clock::time_point created;
    std::string created_by;
    std::string description;
    VolumeStatus status_at_snapshot;
    std::vector<std::string> datasets_at_snapshot;
    uint64_t used_bytes_at_snapshot;
    int mount_count_at_snapshot;
    std::set<std::string> tags_at_snapshot;
    std::string notes_at_snapshot;
};
```

#### LifecycleRecommendation
```cpp
struct LifecycleRecommendation {
    std::string volser;
    LifecycleAction action;         // NONE, WARN, MIGRATE, ARCHIVE, SCRATCH, RETIRE
    std::string reason;
    int priority;                   // 1-10, 10=urgent
    std::chrono::system_clock::time_point due_date;
    bool auto_actionable;
};
```

#### LocationHistoryEntry
```cpp
struct LocationHistoryEntry {
    std::string location;
    std::chrono::system_clock::time_point timestamp;
    std::string moved_by;
    std::string reason;
};
```

### New Enumerations

#### HealthStatus
```cpp
enum class HealthStatus {
    EXCELLENT,  // Score >= 90%
    GOOD,       // Score >= 70%
    FAIR,       // Score >= 50%
    POOR,       // Score >= 30%
    CRITICAL    // Score < 30%
};
```

#### LifecycleAction
```cpp
enum class LifecycleAction {
    NONE,       // No action required
    WARN,       // Warning threshold reached
    MIGRATE,    // Should migrate data
    ARCHIVE,    // Should archive
    SCRATCH,    // Should return to scratch
    RETIRE      // Should retire volume
};
```

### Utility Functions

```cpp
// Calculate Levenshtein edit distance
size_t levenshtein_distance(const std::string& s1, const std::string& s2);

// Check if strings match within threshold
bool fuzzy_match(const std::string& text, const std::string& pattern, size_t threshold = 2);

// Get similarity score (0.0 to 1.0)
double similarity_score(const std::string& s1, const std::string& s2);

// Calculate health score for a volume
VolumeHealthScore calculate_health_score(const TapeVolume& vol);

// Generate unique snapshot ID
std::string generate_snapshot_id(const std::string& volser);

// Convert health status to string
std::string health_status_to_string(HealthStatus status);

// Convert lifecycle action to string
std::string lifecycle_action_to_string(LifecycleAction action);
```

## v3.3.0 API Additions

### Encryption Metadata

```cpp
// Set volume encryption
OperationResult set_volume_encryption(const std::string& volser, 
                                      const EncryptionMetadata& encryption);

// Get volume encryption
EncryptionMetadata get_volume_encryption(const std::string& volser) const;

// Get encrypted/unencrypted volumes
std::vector<TapeVolume> get_encrypted_volumes() const;
std::vector<TapeVolume> get_unencrypted_volumes() const;
```

### Storage Tiering

```cpp
// Set/get volume tier
OperationResult set_volume_tier(const std::string& volser, StorageTier tier);
StorageTier get_volume_tier(const std::string& volser) const;

// Get volumes by tier
std::vector<TapeVolume> get_volumes_by_tier(StorageTier tier) const;

// Auto-tier based on inactivity
BatchResult auto_tier_volumes(int days_inactive = 30);
```

### Quota Management

```cpp
// Pool quotas
OperationResult set_pool_quota(const std::string& pool, const Quota& quota);
std::optional<Quota> get_pool_quota(const std::string& pool) const;

// Owner quotas
OperationResult set_owner_quota(const std::string& owner, const Quota& quota);
std::optional<Quota> get_owner_quota(const std::string& owner) const;

// Quota utilities
bool check_quota_available(const std::string& pool, const std::string& owner, uint64_t bytes) const;
void recalculate_quotas();
std::vector<Quota> get_exceeded_quotas() const;
```

### Audit Log Export

```cpp
// Export audit log in various formats
std::string export_audit_log(AuditExportFormat format) const;
void export_audit_log_to_file(const std::string& filepath, AuditExportFormat format) const;
```

### Configuration Profiles

```cpp
// Profile management
OperationResult save_config_profile(const ConfigProfile& profile);
OperationResult load_config_profile(const std::string& name);
OperationResult delete_config_profile(const std::string& name);
std::vector<ConfigProfile> list_config_profiles() const;
std::optional<ConfigProfile> get_config_profile(const std::string& name) const;
```

### Statistics Aggregation

```cpp
// Aggregate statistics
StatisticsAggregation aggregate_volume_capacity() const;
StatisticsAggregation aggregate_volume_usage() const;
StatisticsAggregation aggregate_volume_health() const;
StatisticsAggregation aggregate_mount_counts() const;
StatisticsAggregation aggregate_error_counts() const;
```

### Parallel Batch Operations

```cpp
// Parallel operations with configurable thread count
BatchResult parallel_add_volumes(const std::vector<TapeVolume>& volumes, size_t thread_count = 4);
BatchResult parallel_delete_volumes(const std::vector<std::string>& volsers, bool force = false, size_t thread_count = 4);
BatchResult parallel_update_volumes(const std::vector<TapeVolume>& volumes, size_t thread_count = 4);
```

### Error Recovery

```cpp
// Retry policy management
void set_retry_policy(const RetryPolicy& policy);
RetryPolicy get_retry_policy() const;
RetryableResult retry_operation(std::function<OperationResult()> operation) const;
```
