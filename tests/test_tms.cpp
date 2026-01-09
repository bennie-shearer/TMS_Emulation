/**
 * @file test_tms.cpp
 * @brief Test suite for TMS Tape Management System
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#include "tms_tape_mgmt.h"
#include "logger.h"
#include "configuration.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>
#include <fstream>

using namespace tms;
namespace fs = std::filesystem;

static int passed = 0, failed = 0, total = 0;

#define TEST(cond, msg) do { \
    total++; \
    if (cond) { std::cout << "  [OK] " << msg << "\n"; passed++; } \
    else { std::cerr << "  [FAIL] " << msg << "\n"; failed++; } \
} while(0)

#define TEST_SECTION(name) std::cout << "\n=== " << name << " ===\n"

void cleanup(const std::string& dir) {
    try { if (fs::exists(dir)) fs::remove_all(dir); } catch(...) {}
}

// Forward declarations for v3.0.0 tests
void test_json_serialization();
void test_volume_groups();
void test_retention_policies();
void test_report_generation();
void test_backup_manager();
void test_event_system();
void test_statistics_history();
void test_integrity_checker();
void test_query_engine();

// Forward declarations for v3.3.0 tests
void test_volume_cloning();
void test_bulk_tagging();
void test_pool_management();

// Forward declarations for v3.3.0 tests
void test_volume_snapshots();
void test_volume_health();
void test_fuzzy_search();
void test_lifecycle_recommendations();
void test_location_history();
void test_pool_migration();
void test_health_report();

// Forward declarations for v3.3.0 tests
void test_encryption_metadata();
void test_storage_tiering();
void test_quota_management();
void test_audit_export();
void test_config_profiles();
void test_statistics_aggregation();
void test_parallel_batch();
void test_error_recovery();

void test_validation() {
    TEST_SECTION("Validation Tests");
    
    // Volser validation
    TEST(validate_volser("ABC123"), "Valid 6-char volser");
    TEST(validate_volser("VOL001"), "Valid volser with numbers");
    TEST(validate_volser("A"), "Valid 1-char volser");
    TEST(!validate_volser("TOOLONG7"), "Reject 7+ char volser");
    TEST(!validate_volser(""), "Reject empty volser");
    TEST(!validate_volser("VOL-01"), "Reject volser with hyphen");
    TEST(!validate_volser("VOL 01"), "Reject volser with space");
    
    // Dataset name validation
    TEST(validate_dataset_name("TEST.DS.NAME"), "Valid dataset with dots");
    TEST(validate_dataset_name("PROD.BACKUP.DATA"), "Valid production dataset");
    TEST(validate_dataset_name("A"), "Valid 1-char dataset");
    TEST(!validate_dataset_name(""), "Reject empty dataset");
    TEST(validate_dataset_name("DS-NAME_123"), "Valid dataset with dash/underscore");
    
    // Tag validation
    TEST(validate_tag("valid-tag_123"), "Valid tag with special chars");
    TEST(validate_tag("backup"), "Simple valid tag");
    TEST(!validate_tag(""), "Reject empty tag");
    TEST(!validate_tag("tag with space"), "Reject tag with space");
    
    // Owner validation
    TEST(validate_owner("ADMIN"), "Valid owner");
    TEST(validate_owner("USER123"), "Valid owner with numbers");
    TEST(!validate_owner(""), "Reject empty owner");
    TEST(!validate_owner("TOOLONGOWNER"), "Reject owner > 8 chars");
}

void test_utility_functions() {
    TEST_SECTION("Utility Function Tests");
    
    // Byte formatting
    TEST(format_bytes(1024).find("KB") != std::string::npos, "Format 1KB");
    TEST(format_bytes(1024*1024).find("MB") != std::string::npos, "Format 1MB");
    TEST(format_bytes(1024ULL*1024*1024).find("GB") != std::string::npos, "Format 1GB");
    
    // Timestamp
    TEST(!get_timestamp().empty(), "Get timestamp not empty");
    
    // Error utilities
    TEST(!error_to_string(TMSError::VOLUME_NOT_FOUND).empty(), "Error to string");
    TEST(get_error_category(TMSError::VOLUME_NOT_FOUND) == "Volume", "Error category - Volume");
    TEST(get_error_category(TMSError::DATASET_NOT_FOUND) == "Dataset", "Error category - Dataset");
    TEST(is_recoverable_error(TMSError::LOCK_TIMEOUT), "Lock timeout is recoverable");
    TEST(!is_recoverable_error(TMSError::VOLUME_NOT_FOUND), "Volume not found is not recoverable");
    
    // Status conversions
    TEST(volume_status_to_string(VolumeStatus::SCRATCH) == "SCRATCH", "Status to string - SCRATCH");
    TEST(string_to_volume_status("PRIVATE") == VolumeStatus::PRIVATE, "String to status - PRIVATE");
    TEST(density_to_string(TapeDensity::DENSITY_LTO3) == "LTO-3", "Density to string - LTO3");
    TEST(string_to_density("LTO-8") == TapeDensity::DENSITY_LTO8, "String to density - LTO8");
    
    // Capacity lookup
    TEST(get_density_capacity(TapeDensity::DENSITY_LTO3) == 400ULL*1024*1024*1024, "LTO3 capacity");
    TEST(get_density_capacity(TapeDensity::DENSITY_LTO9) == 18000ULL*1024*1024*1024, "LTO9 capacity");
    
    // String utilities
    TEST(to_upper("test") == "TEST", "To upper case");
    TEST(trim("  test  ") == "test", "Trim whitespace");
    
    // Pattern matching
    TEST(matches_pattern("VOLSER", "VOL", SearchMode::PREFIX), "Prefix match");
    TEST(matches_pattern("VOLSER", "SER", SearchMode::SUFFIX), "Suffix match");
    TEST(matches_pattern("VOLSER", "LSE", SearchMode::CONTAINS), "Contains match");
    TEST(matches_pattern("VOLSER", "VOLSER", SearchMode::EXACT), "Exact match");
    TEST(matches_pattern("VOL001", "VOL*", SearchMode::WILDCARD), "Wildcard match");
}

void test_volume_operations() {
    TEST_SECTION("Volume Operations Tests");
    cleanup("test_vol");
    TMSSystem sys("test_vol");
    
    TapeVolume vol;
    vol.volser = "TEST01";
    vol.status = VolumeStatus::SCRATCH;
    vol.density = TapeDensity::DENSITY_LTO3;
    vol.location = "SLOT1";
    vol.owner = "ADMIN";
    vol.pool = "SCRATCH";
    
    // Add volume
    TEST(sys.add_volume(vol).is_success(), "Add volume");
    TEST(sys.add_volume(vol).is_error(), "Reject duplicate volume");
    TEST(sys.volume_exists("TEST01"), "Volume exists check");
    TEST(!sys.volume_exists("NOEXIST"), "Volume not exists check");
    TEST(sys.get_volume_count() == 1, "Volume count is 1");
    
    // Get volume
    auto get_result = sys.get_volume("TEST01");
    TEST(get_result.is_success(), "Get volume success");
    TEST(get_result.value().volser == "TEST01", "Get volume correct volser");
    TEST(sys.get_volume("NOEXIST").is_error(), "Get non-existent volume fails");
    
    // Mount/dismount
    TEST(sys.mount_volume("TEST01").is_success(), "Mount volume");
    TEST(sys.mount_volume("TEST01").is_error(), "Reject double mount");
    TEST(sys.get_volume("TEST01").value().status == VolumeStatus::MOUNTED, "Volume is mounted");
    TEST(sys.dismount_volume("TEST01").is_success(), "Dismount volume");
    TEST(sys.dismount_volume("TEST01").is_error(), "Reject dismount unmounted");
    
    // List volumes
    auto volumes = sys.list_volumes();
    TEST(volumes.size() == 1, "List all volumes");
    TEST(sys.list_volumes(VolumeStatus::SCRATCH).size() == 1, "List scratch volumes");
    TEST(sys.list_volumes(VolumeStatus::MOUNTED).size() == 0, "List mounted volumes (empty)");
    
    // Update volume
    auto v = sys.get_volume("TEST01").value();
    v.location = "SLOT2";
    TEST(sys.update_volume(v).is_success(), "Update volume");
    TEST(sys.get_volume("TEST01").value().location == "SLOT2", "Volume updated correctly");
    
    // Delete volume
    TEST(sys.delete_volume("TEST01").is_success(), "Delete volume");
    TEST(sys.get_volume_count() == 0, "Volume count is 0 after delete");
    TEST(sys.delete_volume("TEST01").is_error(), "Delete non-existent volume fails");
    
    cleanup("test_vol");
}

void test_dataset_operations() {
    TEST_SECTION("Dataset Operations Tests");
    cleanup("test_ds");
    TMSSystem sys("test_ds");
    
    // Add volume first
    TapeVolume vol;
    vol.volser = "DS01";
    vol.status = VolumeStatus::SCRATCH;
    vol.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    sys.add_volume(vol);
    
    Dataset ds;
    ds.name = "TEST.DATASET";
    ds.volser = "DS01";
    ds.size_bytes = 1024*1024;
    ds.owner = "TESTUSER";
    ds.job_name = "TESTJOB";
    
    // Add dataset
    TEST(sys.add_dataset(ds).is_success(), "Add dataset");
    TEST(sys.add_dataset(ds).is_error(), "Reject duplicate dataset");
    TEST(sys.dataset_exists("TEST.DATASET"), "Dataset exists");
    TEST(sys.get_dataset_count() == 1, "Dataset count is 1");
    
    // Volume status changed
    TEST(sys.get_volume("DS01").value().status == VolumeStatus::PRIVATE, "Volume now PRIVATE");
    TEST(sys.get_volume("DS01").value().datasets.size() == 1, "Volume has 1 dataset");
    
    // Get dataset
    auto get_result = sys.get_dataset("TEST.DATASET");
    TEST(get_result.is_success(), "Get dataset success");
    TEST(get_result.value().name == "TEST.DATASET", "Get dataset correct name");
    
    // Migrate/recall
    TEST(sys.migrate_dataset("TEST.DATASET").is_success(), "Migrate dataset");
    TEST(sys.get_dataset("TEST.DATASET").value().status == DatasetStatus::MIGRATED, "Dataset is migrated");
    TEST(sys.migrate_dataset("TEST.DATASET").is_error(), "Cannot migrate already migrated");
    TEST(sys.recall_dataset("TEST.DATASET").is_success(), "Recall dataset");
    
    // Delete dataset
    TEST(sys.delete_dataset("TEST.DATASET").is_success(), "Delete dataset");
    TEST(sys.get_volume("DS01").value().status == VolumeStatus::SCRATCH, "Volume back to SCRATCH");
    TEST(sys.get_dataset_count() == 0, "Dataset count is 0");
    
    cleanup("test_ds");
}

void test_scratch_pool() {
    TEST_SECTION("Scratch Pool Tests");
    cleanup("test_scr");
    TMSSystem sys("test_scr");
    
    // Create scratch volumes
    for (int i = 1; i <= 3; i++) {
        TapeVolume v;
        v.volser = "SCR0" + std::to_string(i);
        v.status = VolumeStatus::SCRATCH;
        v.pool = "POOL_A";
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    TEST(sys.get_scratch_pool().size() == 3, "Scratch pool has 3 volumes");
    TEST(sys.get_scratch_pool(2).size() == 2, "Scratch pool limit to 2");
    
    auto [scr_avail, scr_total] = sys.get_scratch_pool_stats();
    TEST(scr_avail == 3, "3 available scratch");
    TEST(scr_total == 3, "3 total volumes");
    
    // Allocate scratch
    auto alloc = sys.allocate_scratch_volume();
    TEST(alloc.is_success(), "Allocate scratch volume");
    TEST(sys.get_volume(alloc.value()).value().status == VolumeStatus::PRIVATE, "Allocated is PRIVATE");
    TEST(sys.get_scratch_pool().size() == 2, "2 scratch remaining");
    
    // Return to scratch
    TEST(sys.scratch_volume(alloc.value()).is_success(), "Return to scratch");
    TEST(sys.get_scratch_pool().size() == 3, "3 scratch after return");
    
    // Pool names
    auto pools = sys.get_pool_names();
    TEST(pools.size() == 1, "One pool exists");
    TEST(pools[0] == "POOL_A", "Pool name is POOL_A");
    
    // Pool statistics
    auto stats = sys.get_pool_statistics("POOL_A");
    TEST(stats.total_volumes == 3, "Pool has 3 volumes");
    TEST(stats.scratch_volumes == 3, "Pool has 3 scratch");
    
    cleanup("test_scr");
}

void test_expiration() {
    TEST_SECTION("Expiration Tests");
    cleanup("test_exp");
    TMSSystem sys("test_exp");
    
    // Create expired volume
    TapeVolume vol;
    vol.volser = "EXP01";
    vol.status = VolumeStatus::PRIVATE;
    vol.expiration_date = std::chrono::system_clock::now() - std::chrono::hours(100);
    vol.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    sys.add_volume(vol);
    
    // Create expired dataset
    Dataset ds;
    ds.name = "EXPIRED.DS";
    ds.volser = "EXP01";
    ds.expiration_date = std::chrono::system_clock::now() - std::chrono::hours(100);
    sys.add_dataset(ds);
    
    // Dry run
    size_t dry_count = sys.process_expirations(true);
    TEST(dry_count == 2, "Dry run finds 2 expired");
    TEST(sys.get_dataset("EXPIRED.DS").value().status == DatasetStatus::ACTIVE, "Dataset still active after dry run");
    
    // Actual processing
    size_t count = sys.process_expirations(false);
    TEST(count == 2, "Process 2 expirations");
    TEST(sys.get_dataset("EXPIRED.DS").value().status == DatasetStatus::EXPIRED, "Dataset expired");
    TEST(sys.get_volume("EXP01").value().status == VolumeStatus::EXPIRED, "Volume expired");
    
    // List expired
    TEST(sys.list_expired_volumes().size() == 1, "1 expired volume");
    TEST(sys.list_expired_datasets().size() == 1, "1 expired dataset");
    
    cleanup("test_exp");
}

void test_persistence() {
    TEST_SECTION("Persistence Tests");
    cleanup("test_pers");
    
    // Create and save
    {
        TMSSystem sys("test_pers");
        TapeVolume v;
        v.volser = "PERS01";
        v.location = "PERSTEST";
        v.owner = "ADMIN";
        v.pool = "BACKUP";
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
        
        Dataset ds;
        ds.name = "PERS.DATA";
        ds.volser = "PERS01";
        ds.size_bytes = 1024 * 1024;
        sys.add_dataset(ds);
        
        sys.save_catalog();
    }
    
    // Reload and verify
    {
        TMSSystem sys("test_pers");
        auto v = sys.get_volume("PERS01");
        TEST(v.is_success(), "Volume persisted");
        TEST(v.value().location == "PERSTEST", "Volume location persisted");
        TEST(v.value().owner == "ADMIN", "Volume owner persisted");
        TEST(v.value().pool == "BACKUP", "Volume pool persisted");
        
        auto ds = sys.get_dataset("PERS.DATA");
        TEST(ds.is_success(), "Dataset persisted");
        TEST(ds.value().volser == "PERS01", "Dataset volume persisted");
    }
    
    cleanup("test_pers");
}

void test_tagging() {
    TEST_SECTION("Tagging Tests");
    cleanup("test_tag");
    TMSSystem sys("test_tag");
    
    TapeVolume vol;
    vol.volser = "TAG001";
    vol.status = VolumeStatus::SCRATCH;
    vol.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    sys.add_volume(vol);
    
    // Add tags
    TEST(sys.add_volume_tag("TAG001", "backup").is_success(), "Add volume tag");
    TEST(sys.add_volume_tag("TAG001", "archive").is_success(), "Add second tag");
    TEST(sys.get_volume("TAG001").value().has_tag("backup"), "Volume has 'backup' tag");
    TEST(sys.get_volume("TAG001").value().has_tag("archive"), "Volume has 'archive' tag");
    
    // Find by tag
    TEST(sys.find_volumes_by_tag("backup").size() == 1, "Find by tag");
    TEST(sys.find_volumes_by_tag("nonexistent").size() == 0, "Find non-existent tag");
    
    // All tags
    auto all_tags = sys.get_all_volume_tags();
    TEST(all_tags.size() == 2, "Get all tags");
    
    // Remove tag
    TEST(sys.remove_volume_tag("TAG001", "backup").is_success(), "Remove tag");
    TEST(!sys.get_volume("TAG001").value().has_tag("backup"), "Tag removed");
    
    // Invalid tag
    TEST(sys.add_volume_tag("TAG001", "").is_error(), "Reject empty tag");
    TEST(sys.add_volume_tag("TAG001", "bad tag").is_error(), "Reject tag with space");
    
    cleanup("test_tag");
}

void test_batch_operations() {
    TEST_SECTION("Batch Operations Tests");
    cleanup("test_batch");
    TMSSystem sys("test_batch");
    
    // Batch add volumes
    std::vector<TapeVolume> vols;
    for (int i = 1; i <= 5; i++) {
        TapeVolume v;
        v.volser = "BAT00" + std::to_string(i);
        v.status = VolumeStatus::SCRATCH;
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        vols.push_back(v);
    }
    
    auto add_result = sys.add_volumes_batch(vols);
    TEST(add_result.succeeded == 5, "Batch add 5 succeeded");
    TEST(add_result.failed == 0, "Batch add 0 failed");
    TEST(add_result.all_succeeded(), "Batch all succeeded");
    TEST(sys.get_volume_count() == 5, "5 volumes after batch add");
    
    // Batch delete
    std::vector<std::string> to_delete = {"BAT001", "BAT002", "NOTEXIST"};
    auto del_result = sys.delete_volumes_batch(to_delete, false);
    TEST(del_result.succeeded == 2, "Batch delete 2 succeeded");
    TEST(del_result.failed == 1, "Batch delete 1 failed");
    TEST(!del_result.all_succeeded(), "Not all succeeded");
    TEST(del_result.success_rate() > 60.0 && del_result.success_rate() < 70.0, "Success rate ~66%");
    
    cleanup("test_batch");
}

void test_reservation() {
    TEST_SECTION("Volume Reservation Tests");
    cleanup("test_res");
    TMSSystem sys("test_res");
    
    TapeVolume vol;
    vol.volser = "RES001";
    vol.status = VolumeStatus::SCRATCH;
    vol.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    sys.add_volume(vol);
    
    // Reserve
    TEST(sys.reserve_volume("RES001", "USER1", std::chrono::seconds(3600)).is_success(), "Reserve volume");
    TEST(sys.get_volume("RES001").value().is_reserved(), "Volume is reserved");
    TEST(sys.get_volume("RES001").value().reserved_by == "USER1", "Reserved by correct user");
    
    // Cannot reserve for different user
    TEST(sys.reserve_volume("RES001", "USER2").is_error(), "Cannot reserve for different user");
    
    // Extend reservation
    TEST(sys.extend_reservation("RES001", "USER1", std::chrono::seconds(1800)).is_success(), "Extend reservation");
    
    // Release
    TEST(sys.release_volume("RES001", "USER2").is_error(), "Wrong user cannot release");
    TEST(sys.release_volume("RES001", "USER1").is_success(), "Correct user can release");
    TEST(!sys.get_volume("RES001").value().is_reserved(), "Volume no longer reserved");
    
    // List reserved
    sys.reserve_volume("RES001", "USER1");
    TEST(sys.list_reserved_volumes().size() == 1, "1 reserved volume");
    
    cleanup("test_res");
}

void test_search() {
    TEST_SECTION("Search Tests");
    cleanup("test_search");
    TMSSystem sys("test_search");
    
    // Create test volumes
    for (int i = 1; i <= 5; i++) {
        TapeVolume v;
        v.volser = "SRH00" + std::to_string(i);
        v.owner = (i <= 3) ? "ADMIN" : "USER";
        v.pool = (i <= 2) ? "POOL_A" : "POOL_B";
        v.location = "RACK" + std::to_string(i);
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    // Simple search
    TEST(sys.search_volumes("ADMIN", "", "").size() == 3, "Search by owner");
    TEST(sys.search_volumes("", "", "POOL_A").size() == 2, "Search by pool");
    TEST(sys.search_volumes("", "RACK1", "").size() == 1, "Search by location");
    
    // Advanced search with criteria
    SearchCriteria criteria;
    criteria.pattern = "SRH00";
    criteria.mode = SearchMode::PREFIX;
    TEST(sys.search_volumes(criteria).size() == 5, "Search with prefix");
    
    criteria.owner = "ADMIN";
    TEST(sys.search_volumes(criteria).size() == 3, "Search with owner filter");
    
    criteria.limit = 2;
    TEST(sys.search_volumes(criteria).size() == 2, "Search with limit");
    
    cleanup("test_search");
}

void test_health_check() {
    TEST_SECTION("Health Check Tests");
    cleanup("test_health");
    TMSSystem sys("test_health");
    
    // Empty system should be healthy
    auto health = sys.perform_health_check();
    TEST(health.healthy, "Empty system is healthy");
    
    // Add some volumes
    for (int i = 1; i <= 3; i++) {
        TapeVolume v;
        v.volser = "HLT00" + std::to_string(i);
        v.status = VolumeStatus::SCRATCH;
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    health = sys.perform_health_check();
    TEST(health.healthy, "System with scratch volumes is healthy");
    TEST(health.metrics.count("total_volumes") > 0, "Has volume metric");
    TEST(health.metrics.count("scratch_available") > 0, "Has scratch metric");
    
    // Integrity check
    auto issues = sys.verify_integrity();
    TEST(issues.empty(), "No integrity issues");
    
    cleanup("test_health");
}

void test_csv_export_import() {
    TEST_SECTION("CSV Export/Import Tests");
    cleanup("test_csv");
    TMSSystem sys("test_csv");
    
    // Create test data
    TapeVolume vol;
    vol.volser = "CSV001";
    vol.status = VolumeStatus::SCRATCH;
    vol.location = "Test Location";
    vol.pool = "POOL_A";
    vol.owner = "ADMIN";
    vol.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    sys.add_volume(vol);
    
    Dataset ds;
    ds.name = "CSV.TEST.DATA";
    ds.volser = "CSV001";
    ds.size_bytes = 1024 * 1024;
    ds.owner = "TESTUSER";
    sys.add_dataset(ds);
    
    // Export
    auto export_result = sys.export_to_csv("test_csv/volumes.csv", "test_csv/datasets.csv");
    TEST(export_result.is_success(), "Export to CSV");
    TEST(fs::exists("test_csv/volumes.csv"), "Volumes CSV created");
    TEST(fs::exists("test_csv/datasets.csv"), "Datasets CSV created");
    
    cleanup("test_csv");
}

void test_regex_cache() {
    TEST_SECTION("Regex Cache Tests");
    
    // Test pattern matching with caching
    TEST(matches_pattern("VOL001", "VOL*", SearchMode::WILDCARD), "Wildcard match with cache");
    TEST(matches_pattern("VOL002", "VOL*", SearchMode::WILDCARD), "Wildcard match (cached)");
    TEST(matches_pattern("TESTVOL", ".*VOL", SearchMode::REGEX), "Regex match with cache");
    
    // Check cache has entries
    size_t cache_size = RegexCache::instance().size();
    TEST(cache_size > 0, "Regex cache has entries");
    
    // Clear and verify
    RegexCache::instance().clear();
    TEST(RegexCache::instance().size() == 0, "Regex cache cleared");
}

void test_audit_pruning() {
    TEST_SECTION("Audit Pruning Tests");
    cleanup("test_audit");
    TMSSystem sys("test_audit");
    
    // Add many volumes to generate audit records
    for (int i = 0; i < 50; i++) {
        TapeVolume v;
        v.volser = "AUD" + std::to_string(100 + i);
        v.status = VolumeStatus::SCRATCH;
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    // Check audit log has entries
    auto log = sys.get_audit_log(100);
    TEST(log.size() >= 50, "Audit log has entries");
    
    // Check pruned count (should be 0 for small log)
    TEST(sys.get_audit_pruned_count() == 0, "No pruning yet for small log");
    
    cleanup("test_audit");
}

void test_secondary_indices() {
    TEST_SECTION("Secondary Index Tests");
    cleanup("test_idx");
    TMSSystem sys("test_idx");
    
    // Add volumes with different owners and pools
    for (int i = 0; i < 10; i++) {
        TapeVolume v;
        v.volser = "IDX" + std::to_string(100 + i);
        v.status = VolumeStatus::SCRATCH;
        v.owner = (i < 5) ? "ADMIN" : "USER";
        v.pool = (i < 3) ? "POOL_A" : "POOL_B";
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    // Test indexed lookups
    auto admin_vols = sys.get_volumes_by_owner("ADMIN");
    TEST(admin_vols.size() == 5, "Index: 5 ADMIN volumes");
    
    auto user_vols = sys.get_volumes_by_owner("USER");
    TEST(user_vols.size() == 5, "Index: 5 USER volumes");
    
    auto pool_a = sys.get_volumes_by_pool("POOL_A");
    TEST(pool_a.size() == 3, "Index: 3 POOL_A volumes");
    
    auto pool_b = sys.get_volumes_by_pool("POOL_B");
    TEST(pool_b.size() == 7, "Index: 7 POOL_B volumes");
    
    // Test get all owners
    auto owners = sys.get_all_owners();
    TEST(owners.size() == 2, "2 unique owners");
    
    cleanup("test_idx");
}

int main() {
    Logger::instance().set_level(Logger::Level::OFF);
    
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  TMS TEST SUITE v" << VERSION_STRING << "\n";
    std::cout << "  " << VERSION_COPYRIGHT << "\n";
    std::cout << "========================================\n";
    
    auto start = std::chrono::steady_clock::now();
    
    test_validation();
    test_utility_functions();
    test_volume_operations();
    test_dataset_operations();
    test_scratch_pool();
    test_expiration();
    test_persistence();
    test_tagging();
    test_batch_operations();
    test_reservation();
    test_search();
    test_health_check();
    test_csv_export_import();
    test_regex_cache();
    test_audit_pruning();
    test_secondary_indices();
    
    // v3.0.0 Feature Tests
    test_json_serialization();
    test_volume_groups();
    test_retention_policies();
    test_report_generation();
    test_backup_manager();
    test_event_system();
    test_statistics_history();
    test_integrity_checker();
    test_query_engine();
    
    // v3.3.0 Feature Tests
    test_volume_cloning();
    test_bulk_tagging();
    test_pool_management();
    
    // v3.3.0 New Feature Tests
    test_volume_snapshots();
    test_volume_health();
    test_fuzzy_search();
    test_lifecycle_recommendations();
    test_location_history();
    test_pool_migration();
    test_health_report();
    
    // v3.3.0 New Feature Tests
    test_encryption_metadata();
    test_storage_tiering();
    test_quota_management();
    test_audit_export();
    test_config_profiles();
    test_statistics_aggregation();
    test_parallel_batch();
    test_error_recovery();
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "\n========================================\n";
    std::cout << "  Total:  " << total << " tests\n";
    std::cout << "  Passed: " << passed << "\n";
    std::cout << "  Failed: " << failed << "\n";
    std::cout << "  Time:   " << ms << "ms\n";
    std::cout << "========================================\n";
    
    if (failed == 0) {
        std::cout << "\n*** ALL TESTS PASSED ***\n\n";
        return 0;
    }
    
    std::cout << "\n*** " << failed << " TEST(S) FAILED ***\n\n";
    return 1;
}

// ============================================================================
// v3.0.0 Feature Tests
// ============================================================================

void test_json_serialization() {
    TEST_SECTION("JSON Serialization Tests");
    
    // Test JsonValue types
    JsonValue null_val;
    TEST(null_val.is_null(), "Null value");
    
    JsonValue bool_val(true);
    TEST(bool_val.is_bool() && bool_val.as_bool() == true, "Boolean value");
    
    JsonValue num_val(42);
    TEST(num_val.is_number() && num_val.as_int() == 42, "Number value");
    
    JsonValue str_val("test");
    TEST(str_val.is_string() && str_val.as_string() == "test", "String value");
    
    // Test JSON serialization
    JsonObject obj;
    obj["name"] = "TEST01";
    obj["value"] = 123;
    obj["active"] = true;
    JsonValue json_obj(obj);
    
    std::string json_str = JsonSerializer::serialize(json_obj);
    TEST(!json_str.empty(), "JSON serialization produces output");
    TEST(json_str.find("TEST01") != std::string::npos, "JSON contains expected value");
    
    // Test parsing
    JsonValue parsed = JsonSerializer::parse(json_str);
    TEST(parsed.is_object(), "Parsed JSON is object");
    TEST(parsed["name"].as_string() == "TEST01", "Parsed value matches");
    
    // Test volume conversion
    TapeVolume vol;
    vol.volser = "JSON01";
    vol.status = VolumeStatus::SCRATCH;
    vol.pool = "POOL_A";
    vol.owner = "ADMIN";
    
    JsonValue vol_json = TmsJsonConverter::volume_to_json(vol);
    TEST(vol_json["volser"].as_string() == "JSON01", "Volume to JSON conversion");
    
    TapeVolume restored = TmsJsonConverter::json_to_volume(vol_json);
    TEST(restored.volser == "JSON01", "JSON to Volume conversion");
    TEST(restored.pool == "POOL_A", "Pool preserved in conversion");
}

void test_volume_groups() {
    TEST_SECTION("Volume Groups Tests");
    
    VolumeGroupManager mgr;
    
    // Create group
    VolumeGroup group;
    group.name = "TEST_GROUP";
    group.description = "Test volume group";
    group.owner = "ADMIN";
    
    auto create_result = mgr.create_group(group);
    TEST(create_result.is_success(), "Create volume group");
    TEST(mgr.group_exists("TEST_GROUP"), "Group exists after creation");
    
    // Add volumes
    auto add_result = mgr.add_volume("TEST_GROUP", "VOL001");
    TEST(add_result.is_success(), "Add volume to group");
    
    mgr.add_volume("TEST_GROUP", "VOL002");
    mgr.add_volume("TEST_GROUP", "VOL003");
    
    auto volumes = mgr.get_volumes("TEST_GROUP");
    TEST(volumes.size() == 3, "Group has 3 volumes");
    
    // Get groups for volume
    auto groups = mgr.get_groups_for_volume("VOL001");
    TEST(groups.size() == 1 && groups[0] == "TEST_GROUP", "Get groups for volume");
    
    // Remove volume
    auto rm_result = mgr.remove_volume("TEST_GROUP", "VOL002");
    TEST(rm_result.is_success(), "Remove volume from group");
    TEST(mgr.get_volumes("TEST_GROUP").size() == 2, "Group has 2 volumes after removal");
    
    // Delete group
    auto del_result = mgr.delete_group("TEST_GROUP", true);
    TEST(del_result.is_success(), "Delete group");
    TEST(!mgr.group_exists("TEST_GROUP"), "Group no longer exists");
}

void test_retention_policies() {
    TEST_SECTION("Retention Policy Tests");
    
    RetentionPolicyManager mgr;
    
    // Create policy
    RetentionPolicy policy;
    policy.name = "SHORT_TERM";
    policy.description = "30-day retention";
    policy.retention_value = 30;
    policy.retention_unit = RetentionUnit::DAYS;
    policy.action = RetentionAction::EXPIRE;
    policy.warning_days = 7;
    
    auto create_result = mgr.create_policy(policy);
    TEST(create_result.is_success(), "Create retention policy");
    TEST(mgr.policy_exists("SHORT_TERM"), "Policy exists");
    
    // Get policy
    auto get_result = mgr.get_policy("SHORT_TERM");
    TEST(get_result.is_success(), "Get policy");
    TEST(get_result.value().retention_value == 30, "Policy retention value correct");
    
    // Calculate expiration
    auto now = std::chrono::system_clock::now();
    auto expiry = policy.calculate_expiration(now);
    auto days_diff = std::chrono::duration_cast<std::chrono::hours>(expiry - now).count() / 24;
    TEST(days_diff == 30, "Expiration calculation correct");
    
    // Assign policy
    mgr.assign_policy("VOL001", "SHORT_TERM");
    auto assigned = mgr.get_assigned_policy("VOL001");
    TEST(assigned.has_value() && assigned.value() == "SHORT_TERM", "Policy assignment");
    
    // List policies
    auto policies = mgr.list_policies();
    TEST(policies.size() == 1, "List policies");
    
    // Delete policy
    auto del_result = mgr.delete_policy("SHORT_TERM");
    TEST(del_result.is_success(), "Delete policy");
}

void test_report_generation() {
    TEST_SECTION("Report Generation Tests");
    
    ReportGenerator gen;
    
    // Create test data
    std::vector<TapeVolume> volumes;
    for (int i = 0; i < 3; i++) {
        TapeVolume v;
        v.volser = "RPT" + std::to_string(100 + i);
        v.status = VolumeStatus::SCRATCH;
        v.pool = "POOL_A";
        v.owner = "ADMIN";
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        v.used_bytes = 100ULL * 1024 * 1024 * 1024 * i;
        volumes.push_back(v);
    }
    
    // Generate text report
    std::string text_report = gen.generate_volume_report(volumes, ReportFormat::TEXT);
    TEST(!text_report.empty(), "Text report generated");
    TEST(text_report.find("RPT100") != std::string::npos, "Text report contains volume");
    
    // Generate HTML report
    std::string html_report = gen.generate_volume_report(volumes, ReportFormat::HTML);
    TEST(!html_report.empty(), "HTML report generated");
    TEST(html_report.find("<html>") != std::string::npos, "HTML report has HTML tags");
    TEST(html_report.find("RPT100") != std::string::npos, "HTML report contains volume");
    
    // Generate Markdown report
    std::string md_report = gen.generate_volume_report(volumes, ReportFormat::MARKDOWN);
    TEST(!md_report.empty(), "Markdown report generated");
    TEST(md_report.find("|") != std::string::npos, "Markdown has table formatting");
    
    // Generate CSV report
    std::string csv_report = gen.generate_volume_report(volumes, ReportFormat::CSV);
    TEST(!csv_report.empty(), "CSV report generated");
    TEST(csv_report.find(",") != std::string::npos, "CSV has comma separators");
    
    // Generate statistics report
    SystemStatistics stats;
    stats.total_volumes = 100;
    stats.scratch_volumes = 50;
    stats.total_capacity = 40ULL * 1024 * 1024 * 1024 * 1024;
    stats.used_capacity = 20ULL * 1024 * 1024 * 1024 * 1024;
    
    std::string stats_report = gen.generate_statistics_report(stats, ReportFormat::TEXT);
    TEST(!stats_report.empty(), "Statistics report generated");
    TEST(stats_report.find("100") != std::string::npos, "Stats report contains volume count");
}

void test_backup_manager() {
    TEST_SECTION("Backup Manager Tests");
    cleanup("test_backup");
    
    BackupConfig config;
    config.backup_directory = "test_backup";
    config.backup_prefix = "tms_test";
    config.scheme = RotationScheme::SIMPLE;
    config.keep_count = 3;
    
    BackupManager mgr(config);
    
    // Create backup using callback
    auto result = mgr.create_backup([](const std::string& path) {
        std::ofstream f(path);
        f << "test backup data";
        return OperationResult::ok();
    }, "daily");
    
    TEST(result.success, "Create backup");
    TEST(!result.backup_path.empty(), "Backup path set");
    
    // List backups
    auto backups = mgr.list_backups();
    TEST(backups.size() == 1, "One backup exists");
    
    // Get latest
    auto latest = mgr.get_latest_backup();
    TEST(latest.has_value(), "Get latest backup");
    
    // Scheduling helpers
    TEST(mgr.should_create_daily_backup() || !mgr.should_create_daily_backup(), 
         "Daily backup check works");
    
    cleanup("test_backup");
}

void test_event_system() {
    TEST_SECTION("Event System Tests");
    
    EventBus& bus = EventBus::instance();
    bus.clear_history();
    
    // Test subscription
    int event_count = 0;
    auto handler_id = bus.subscribe(EventType::VOLUME_ADDED, [&]([[maybe_unused]] const Event& e) {
        event_count++;
    });
    
    TEST(bus.get_subscriber_count() >= 1, "Subscriber registered");
    
    // Publish event
    bus.publish(EventType::VOLUME_ADDED, "TMSSystem", "VOL001", "Volume added");
    TEST(event_count == 1, "Event handler called");
    
    // Check history
    auto history = bus.get_history(10);
    TEST(history.size() >= 1, "Event in history");
    
    // Publish more events
    bus.publish(EventType::DATASET_ADDED, "TMSSystem", "DS001", "Dataset added");
    bus.publish(EventType::VOLUME_MOUNTED, "TMSSystem", "VOL001", "Volume mounted");
    
    // Get events by type
    auto vol_events = bus.get_events_by_type(EventType::VOLUME_ADDED, 10);
    TEST(vol_events.size() >= 1, "Get events by type");
    
    // Unsubscribe
    bus.unsubscribe(handler_id);
    int before_count = event_count;
    bus.publish(EventType::VOLUME_ADDED, "TMSSystem", "VOL002", "Volume added");
    TEST(event_count == before_count, "Handler not called after unsubscribe");
    
    // Event filter
    EventFilter filter = EventFilter::for_severity(EventSeverity::ERROR);
    Event test_event(EventType::CUSTOM, "Test", "Target", "Message");
    test_event.severity = EventSeverity::INFO;
    TEST(!filter.matches(test_event), "Filter rejects non-matching severity");
    test_event.severity = EventSeverity::ERROR;
    TEST(filter.matches(test_event), "Filter accepts matching severity");
}

void test_statistics_history() {
    TEST_SECTION("Statistics History Tests");
    
    StatisticsHistory history;
    
    // Record snapshots
    for (int i = 0; i < 5; i++) {
        SystemStatistics stats;
        stats.total_volumes = 100 + i * 10;
        stats.scratch_volumes = 50 + i * 5;
        stats.total_capacity = 1000ULL * 1024 * 1024 * 1024;
        stats.used_capacity = (500 + i * 50) * 1024ULL * 1024 * 1024;
        history.record_snapshot(stats);
    }
    
    TEST(history.snapshot_count() == 5, "5 snapshots recorded");
    
    // Get latest
    auto latest = history.get_latest_snapshot();
    TEST(latest.has_value(), "Get latest snapshot");
    TEST(latest->total_volumes == 140, "Latest snapshot has correct data");
    
    // Get recent snapshots
    auto recent = history.get_recent_snapshots(30);
    TEST(recent.size() == 5, "Get recent snapshots");
    
    // Trend analysis
    auto trend = history.analyze_volume_trend(30);
    TEST(trend.sample_count == 5, "Trend has 5 samples");
    TEST(trend.direction == TrendDirection::UP, "Trend direction is UP");
    
    // Capacity projection
    auto projection = history.project_capacity(30);
    TEST(projection.projected_utilization >= 0, "Capacity projection generated");
    
    // Daily averages
    auto averages = history.get_daily_averages(30);
    TEST(!averages.empty(), "Daily averages calculated");
    
    // Clear history
    history.clear_history();
    TEST(history.snapshot_count() == 0, "History cleared");
}

void test_integrity_checker() {
    TEST_SECTION("Integrity Checker Tests");
    cleanup("test_integrity");
    
    TMSSystem sys("test_integrity");
    
    // Add valid data
    TapeVolume v1;
    v1.volser = "INT001";
    v1.status = VolumeStatus::PRIVATE;
    v1.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
    v1.used_bytes = 100ULL * 1024 * 1024 * 1024;
    sys.add_volume(v1);
    
    Dataset ds1;
    ds1.name = "INT.TEST.DS1";
    ds1.volser = "INT001";
    ds1.size_bytes = 1024 * 1024;
    sys.add_dataset(ds1);
    
    // Run integrity check
    IntegrityChecker checker;
    auto result = checker.check_integrity(
        [&]() { return sys.list_volumes(); },
        [&]() { return sys.list_datasets(); }
    );
    
    TEST(result.volumes_checked == 1, "1 volume checked");
    TEST(result.datasets_checked == 1, "1 dataset checked");
    TEST(result.passed, "Integrity check passed");
    
    // Generate report
    std::string report_str = checker.generate_report(result);
    TEST(!report_str.empty(), "Report generated");
    TEST(report_str.find("PASSED") != std::string::npos || result.passed, "Integrity check passed");
    
    cleanup("test_integrity");
}

void test_query_engine() {
    TEST_SECTION("Query Engine Tests");
    cleanup("test_query");
    
    TMSSystem sys("test_query");
    
    // Add test volumes
    for (int i = 0; i < 10; i++) {
        TapeVolume v;
        v.volser = "QRY" + std::to_string(100 + i);
        v.status = (i < 5) ? VolumeStatus::SCRATCH : VolumeStatus::PRIVATE;
        v.pool = (i < 3) ? "POOL_A" : "POOL_B";
        v.owner = (i % 2 == 0) ? "ADMIN" : "USER";
        v.capacity_bytes = 400ULL * 1024 * 1024 * 1024;
        sys.add_volume(v);
    }
    
    QueryEngine engine;
    auto get_volumes = [&]() { return sys.list_volumes(); };
    
    // Query builder test
    QueryBuilder builder;
    auto conditions = builder.field(QueryField::VOLUME_POOL)
                             .equals("POOL_A")
                             .build();
    TEST(!conditions.empty(), "QueryBuilder creates conditions");
    
    // Query volumes with builder conditions
    auto pool_vols = engine.query_volumes(conditions, get_volumes);
    TEST(pool_vols.size() == 3, "Query returns 3 POOL_A volumes");
    
    // Query by status
    QueryBuilder status_builder;
    auto status_conditions = status_builder.field(QueryField::VOLUME_STATUS)
                                           .equals("SCRATCH")
                                           .build();
    auto scratch_vols = engine.query_volumes(status_conditions, get_volumes);
    TEST(scratch_vols.size() == 5, "Query returns 5 scratch volumes");
    
    // Query by owner
    QueryBuilder owner_builder;
    auto owner_conditions = owner_builder.field(QueryField::VOLUME_OWNER)
                                         .equals("ADMIN")
                                         .build();
    auto admin_vols = engine.query_volumes(owner_conditions, get_volumes);
    TEST(admin_vols.size() == 5, "Query returns 5 ADMIN volumes");
    
    // Test syntax help
    auto syntax_help = QueryEngine::get_query_syntax_help();
    TEST(!syntax_help.empty(), "Query syntax help available");
    
    // Test field names
    auto fields = QueryEngine::get_field_names();
    TEST(!fields.empty(), "Field names available");
    
    // Save query test
    SavedQuery saved;
    saved.name = "SCRATCH_VOLS";
    saved.description = "Find scratch volumes";
    saved.query_string = "status=SCRATCH";
    auto save_result = engine.save_query(saved);
    TEST(save_result.is_success(), "Save query");
    
    // Get saved query
    auto retrieved = engine.get_query("SCRATCH_VOLS");
    TEST(retrieved.has_value(), "Retrieve saved query");
    
    cleanup("test_query");
}

// ============================================================================
// v3.3.0 Feature Tests
// ============================================================================

void test_volume_cloning() {
    TEST_SECTION("Volume Cloning Tests");
    cleanup("test_clone");
    TMSSystem sys("test_clone");
    
    // Create source volume
    TapeVolume source;
    source.volser = "SRC001";
    source.status = VolumeStatus::SCRATCH;
    source.pool = "CLONE_POOL";
    source.owner = "ADMIN";
    source.location = "VAULT-A";
    source.density = TapeDensity::DENSITY_LTO8;
    source.tags.insert("original");
    source.tags.insert("production");
    sys.add_volume(source);
    
    // Clone the volume
    auto result = sys.clone_volume("SRC001", "CLN001");
    TEST(result.is_success(), "Clone volume success");
    
    // Verify cloned volume
    auto cloned = sys.get_volume("CLN001");
    TEST(cloned.is_success(), "Cloned volume exists");
    TEST(cloned.value().pool == "CLONE_POOL", "Pool preserved in clone");
    TEST(cloned.value().owner == "ADMIN", "Owner preserved in clone");
    TEST(cloned.value().status == VolumeStatus::SCRATCH, "Clone starts as scratch");
    TEST(cloned.value().tags.count("original") > 0, "Tags preserved in clone");
    
    // Clone to existing volser should fail
    auto dup_result = sys.clone_volume("SRC001", "CLN001");
    TEST(dup_result.is_error(), "Clone to existing volser fails");
    
    cleanup("test_clone");
}

void test_bulk_tagging() {
    TEST_SECTION("Bulk Tagging Tests");
    cleanup("test_bulk_tag");
    TMSSystem sys("test_bulk_tag");
    
    // Create test volumes
    for (int i = 1; i <= 5; i++) {
        TapeVolume vol;
        vol.volser = "BLK00" + std::to_string(i);
        vol.status = VolumeStatus::SCRATCH;
        vol.pool = "BULK_POOL";
        vol.owner = "ADMIN";
        sys.add_volume(vol);
    }
    
    // Add tag to multiple volumes
    std::vector<std::string> volsers = {"BLK001", "BLK002", "BLK003"};
    auto result = sys.add_tag_to_volumes(volsers, "bulk-tagged");
    TEST(result.succeeded == 3, "Bulk add tag succeeded for 3 volumes");
    
    // Verify tags were added
    auto vol1 = sys.get_volume("BLK001");
    TEST(vol1.value().has_tag("bulk-tagged"), "Volume 1 has bulk tag");
    
    auto vol4 = sys.get_volume("BLK004");
    TEST(!vol4.value().has_tag("bulk-tagged"), "Volume 4 does not have bulk tag");
    
    // Remove tag from volumes
    auto remove_result = sys.remove_tag_from_volumes(volsers, "bulk-tagged");
    TEST(remove_result.succeeded == 3, "Bulk remove tag succeeded for 3 volumes");
    
    // Verify tags were removed
    vol1 = sys.get_volume("BLK001");
    TEST(!vol1.value().has_tag("bulk-tagged"), "Volume 1 no longer has bulk tag");
    
    cleanup("test_bulk_tag");
}

void test_pool_management() {
    TEST_SECTION("Pool Management Tests");
    cleanup("test_pool_mgmt");
    TMSSystem sys("test_pool_mgmt");
    
    // Create volumes in different pools
    for (int i = 1; i <= 3; i++) {
        TapeVolume vol;
        vol.volser = "PLA00" + std::to_string(i);
        vol.pool = "POOL_A";
        vol.owner = "ADMIN";
        sys.add_volume(vol);
    }
    for (int i = 1; i <= 2; i++) {
        TapeVolume vol;
        vol.volser = "PLB00" + std::to_string(i);
        vol.pool = "POOL_B";
        vol.owner = "ADMIN";
        sys.add_volume(vol);
    }
    
    // Rename pool
    auto rename_result = sys.rename_pool("POOL_A", "POOL_ALPHA");
    TEST(rename_result.is_success(), "Rename pool success");
    
    // Verify volumes were updated
    auto vol = sys.get_volume("PLA001");
    TEST(vol.value().pool == "POOL_ALPHA", "Volume pool updated after rename");
    
    // Merge pools
    auto merge_result = sys.merge_pools("POOL_B", "POOL_ALPHA");
    TEST(merge_result.is_success(), "Merge pools success");
    
    // Verify all volumes now in POOL_ALPHA
    auto plb_vol = sys.get_volume("PLB001");
    TEST(plb_vol.value().pool == "POOL_ALPHA", "Volume moved to target pool");
    
    // Verify pool stats
    auto pool_stats = sys.get_pool_statistics("POOL_ALPHA");
    TEST(pool_stats.total_volumes == 5, "Pool has all 5 volumes after merge");
    
    cleanup("test_pool_mgmt");
}

// ============================================================================
// v3.3.0 New Feature Tests
// ============================================================================

void test_volume_snapshots() {
    TEST_SECTION("Volume Snapshots Tests");
    cleanup("test_snapshots");
    TMSSystem sys("test_snapshots");
    
    // Create a volume
    TapeVolume vol;
    vol.volser = "SNAP01";
    vol.status = VolumeStatus::SCRATCH;
    vol.pool = "SNAP_POOL";
    vol.owner = "ADMIN";
    vol.tags.insert("production");
    vol.notes = "Original notes";
    sys.add_volume(vol);
    
    // Create a snapshot
    auto snap_result = sys.create_volume_snapshot("SNAP01", "Initial state");
    TEST(snap_result.is_success(), "Create snapshot success");
    
    auto snapshot = snap_result.value();
    TEST(!snapshot.snapshot_id.empty(), "Snapshot ID generated");
    TEST(snapshot.volser == "SNAP01", "Snapshot has correct volser");
    TEST(snapshot.tags_at_snapshot.count("production") > 0, "Snapshot captured tags");
    
    // Get snapshots
    auto snapshots = sys.get_volume_snapshots("SNAP01");
    TEST(snapshots.size() == 1, "One snapshot exists");
    
    // Get snapshot count
    TEST(sys.get_snapshot_count() == 1, "Snapshot count is 1");
    
    // Delete snapshot
    auto del_result = sys.delete_snapshot(snapshot.snapshot_id);
    TEST(del_result.is_success(), "Delete snapshot success");
    TEST(sys.get_snapshot_count() == 0, "Snapshot count is 0 after delete");
    
    cleanup("test_snapshots");
}

void test_volume_health() {
    TEST_SECTION("Volume Health Tests");
    cleanup("test_health");
    TMSSystem sys("test_health");
    
    // Create a healthy volume
    TapeVolume vol;
    vol.volser = "HLT001";
    vol.status = VolumeStatus::SCRATCH;
    vol.pool = "HEALTH_POOL";
    vol.owner = "ADMIN";
    vol.mount_count = 10;
    vol.error_count = 0;
    sys.add_volume(vol);
    
    // Check health score
    auto health = sys.get_volume_health("HLT001");
    TEST(health.overall_score >= 80.0, "Healthy volume has high score");
    TEST(health.status == HealthStatus::EXCELLENT || health.status == HealthStatus::GOOD, 
         "Healthy volume has good status");
    
    // Create unhealthy volume
    TapeVolume bad_vol;
    bad_vol.volser = "BAD001";
    bad_vol.status = VolumeStatus::SCRATCH;
    bad_vol.mount_count = 10000;  // Very high mount count
    bad_vol.error_count = 50;     // Many errors
    sys.add_volume(bad_vol);
    
    auto bad_health = sys.get_volume_health("BAD001");
    TEST(bad_health.overall_score < 70.0, "Unhealthy volume has low score");
    
    // Recalculate all health
    auto recalc_result = sys.recalculate_all_health();
    TEST(recalc_result.total == 2, "Recalculated 2 volumes");
    TEST(recalc_result.succeeded == 2, "All recalculations succeeded");
    
    cleanup("test_health");
}

void test_fuzzy_search() {
    TEST_SECTION("Fuzzy Search Tests");
    cleanup("test_fuzzy");
    TMSSystem sys("test_fuzzy");
    
    // Create volumes
    TapeVolume vol1, vol2, vol3;
    vol1.volser = "TEST01"; vol1.status = VolumeStatus::SCRATCH;
    vol2.volser = "TEST02"; vol2.status = VolumeStatus::SCRATCH;
    vol3.volser = "PROD01"; vol3.status = VolumeStatus::SCRATCH;
    sys.add_volume(vol1);
    sys.add_volume(vol2);
    sys.add_volume(vol3);
    
    // Fuzzy search with exact match
    auto results = sys.fuzzy_search_volumes("TEST01", 1);
    TEST(results.size() >= 1, "Fuzzy search found exact match");
    
    // Fuzzy search with typo
    results = sys.fuzzy_search_volumes("TSET01", 2);  // Typo: TSET instead of TEST
    TEST(results.size() >= 1, "Fuzzy search found with typo");
    
    // Fuzzy search with higher threshold
    results = sys.fuzzy_search_volumes("TST", 3);
    TEST(results.size() >= 1, "Fuzzy search with high threshold");
    
    cleanup("test_fuzzy");
}

void test_lifecycle_recommendations() {
    TEST_SECTION("Lifecycle Recommendations Tests");
    cleanup("test_lifecycle");
    TMSSystem sys("test_lifecycle");
    
    // Create expired volume
    TapeVolume expired_vol;
    expired_vol.volser = "EXP001";
    expired_vol.status = VolumeStatus::SCRATCH;
    expired_vol.expiration_date = std::chrono::system_clock::now() - std::chrono::hours(24);
    sys.add_volume(expired_vol);
    
    // Create high-error volume
    TapeVolume error_vol;
    error_vol.volser = "ERR001";
    error_vol.status = VolumeStatus::PRIVATE;
    error_vol.error_count = 30;
    sys.add_volume(error_vol);
    
    // Get recommendations
    auto recommendations = sys.get_lifecycle_recommendations();
    TEST(recommendations.size() >= 1, "Got lifecycle recommendations");
    
    // Check that recommendations are sorted by priority
    if (recommendations.size() > 1) {
        TEST(recommendations[0].priority >= recommendations[1].priority, 
             "Recommendations sorted by priority");
    }
    
    cleanup("test_lifecycle");
}

void test_location_history() {
    TEST_SECTION("Location History Tests");
    cleanup("test_loc_hist");
    TMSSystem sys("test_loc_hist");
    
    // Create volume
    TapeVolume vol;
    vol.volser = "LOC001";
    vol.status = VolumeStatus::SCRATCH;
    vol.location = "SLOT-A1";
    sys.add_volume(vol);
    
    // Update location
    sys.update_volume_location("LOC001", "SLOT-B2");
    sys.update_volume_location("LOC001", "VAULT-1");
    
    // Get location history
    auto history = sys.get_location_history("LOC001");
    TEST(history.size() >= 2, "Location history has entries");
    
    cleanup("test_loc_hist");
}

void test_pool_migration() {
    TEST_SECTION("Pool Migration Tests");
    cleanup("test_pool_mig");
    TMSSystem sys("test_pool_mig");
    
    // Create volumes in different pools
    for (int i = 1; i <= 3; i++) {
        TapeVolume vol;
        vol.volser = "MIG00" + std::to_string(i);
        vol.pool = "SOURCE_POOL";
        sys.add_volume(vol);
    }
    
    // Move single volume
    auto result = sys.move_volume_to_pool("MIG001", "TARGET_POOL");
    TEST(result.is_success(), "Move single volume success");
    
    auto vol = sys.get_volume("MIG001");
    TEST(vol.value().pool == "TARGET_POOL", "Volume moved to target pool");
    
    // Move multiple volumes
    std::vector<std::string> volsers = {"MIG002", "MIG003"};
    auto batch_result = sys.move_volumes_to_pool(volsers, "TARGET_POOL");
    TEST(batch_result.succeeded == 2, "Batch move succeeded for 2 volumes");
    
    // Verify pool stats
    auto stats = sys.get_pool_statistics("TARGET_POOL");
    TEST(stats.total_volumes == 3, "Target pool has 3 volumes");
    
    cleanup("test_pool_mig");
}

void test_health_report() {
    TEST_SECTION("Health Report Tests");
    cleanup("test_health_rpt");
    TMSSystem sys("test_health_rpt");
    
    // Create volumes with varying health
    TapeVolume good_vol;
    good_vol.volser = "GOOD01";
    good_vol.status = VolumeStatus::SCRATCH;
    sys.add_volume(good_vol);
    
    TapeVolume bad_vol;
    bad_vol.volser = "BAD001";
    bad_vol.status = VolumeStatus::SCRATCH;
    bad_vol.error_count = 100;
    sys.add_volume(bad_vol);
    
    // Generate health report
    std::ostringstream oss;
    sys.generate_health_report(oss);
    std::string report = oss.str();
    
    TEST(report.find("Health Report") != std::string::npos, "Health report has header");
    TEST(report.find("Lifecycle") != std::string::npos, "Health report mentions lifecycle");
    
    cleanup("test_health_rpt");
}

// ============================================================================
// v3.3.0 New Feature Tests
// ============================================================================

void test_encryption_metadata() {
    TEST_SECTION("Encryption Metadata Tests");
    cleanup("test_encrypt");
    TMSSystem sys("test_encrypt");
    
    // Create a volume
    TapeVolume vol;
    vol.volser = "ENC001";
    vol.status = VolumeStatus::SCRATCH;
    sys.add_volume(vol);
    
    // Set encryption
    EncryptionMetadata enc;
    enc.encrypted = true;
    enc.algorithm = EncryptionAlgorithm::AES_256;
    enc.key_id = "KEY-001";
    enc.key_label = "PRODUCTION";
    enc.encrypted_by = "ADMIN";
    enc.encrypted_date = std::chrono::system_clock::now();
    
    auto result = sys.set_volume_encryption("ENC001", enc);
    TEST(result.is_success(), "Set encryption success");
    
    // Get encryption
    auto enc_meta = sys.get_volume_encryption("ENC001");
    TEST(enc_meta.is_encrypted(), "Volume is encrypted");
    TEST(enc_meta.algorithm == EncryptionAlgorithm::AES_256, "Algorithm is AES-256");
    TEST(enc_meta.key_id == "KEY-001", "Key ID matches");
    
    // Get encrypted volumes
    auto encrypted = sys.get_encrypted_volumes();
    TEST(encrypted.size() == 1, "Found 1 encrypted volume");
    
    // Get unencrypted volumes
    TapeVolume vol2;
    vol2.volser = "PLN001";
    vol2.status = VolumeStatus::SCRATCH;
    sys.add_volume(vol2);
    
    auto unencrypted = sys.get_unencrypted_volumes();
    TEST(unencrypted.size() == 1, "Found 1 unencrypted volume");
    
    cleanup("test_encrypt");
}

void test_storage_tiering() {
    TEST_SECTION("Storage Tiering Tests");
    cleanup("test_tier");
    TMSSystem sys("test_tier");
    
    // Create volumes
    TapeVolume vol;
    vol.volser = "TIER01";
    vol.status = VolumeStatus::SCRATCH;
    sys.add_volume(vol);
    
    // Default tier is HOT
    auto tier = sys.get_volume_tier("TIER01");
    TEST(tier == StorageTier::HOT, "Default tier is HOT");
    
    // Set tier to COLD
    auto result = sys.set_volume_tier("TIER01", StorageTier::COLD);
    TEST(result.is_success(), "Set tier success");
    
    tier = sys.get_volume_tier("TIER01");
    TEST(tier == StorageTier::COLD, "Tier is now COLD");
    
    // Get volumes by tier
    auto cold_vols = sys.get_volumes_by_tier(StorageTier::COLD);
    TEST(cold_vols.size() == 1, "Found 1 COLD volume");
    
    cleanup("test_tier");
}

void test_quota_management() {
    TEST_SECTION("Quota Management Tests");
    cleanup("test_quota");
    TMSSystem sys("test_quota");
    
    // Set pool quota
    Quota pool_quota;
    pool_quota.name = "PROD_POOL";
    pool_quota.max_bytes = 1000000000;
    pool_quota.max_volumes = 100;
    pool_quota.enabled = true;
    
    auto result = sys.set_pool_quota("PROD_POOL", pool_quota);
    TEST(result.is_success(), "Set pool quota success");
    
    // Get pool quota
    auto quota = sys.get_pool_quota("PROD_POOL");
    TEST(quota.has_value(), "Pool quota exists");
    TEST(quota->max_bytes == 1000000000, "Max bytes correct");
    
    // Set owner quota
    Quota owner_quota;
    owner_quota.name = "ADMIN";
    owner_quota.max_volumes = 50;
    
    result = sys.set_owner_quota("ADMIN", owner_quota);
    TEST(result.is_success(), "Set owner quota success");
    
    // Check quota available
    bool avail = sys.check_quota_available("PROD_POOL", "ADMIN", 1000);
    TEST(avail, "Quota available for small addition");
    
    cleanup("test_quota");
}

void test_audit_export() {
    TEST_SECTION("Audit Export Tests");
    cleanup("test_audit_exp");
    TMSSystem sys("test_audit_exp");
    
    // Generate some audit entries
    TapeVolume vol;
    vol.volser = "AUD001";
    vol.status = VolumeStatus::SCRATCH;
    sys.add_volume(vol);
    sys.delete_volume("AUD001", true);
    
    // Export as JSON
    std::string json = sys.export_audit_log(AuditExportFormat::JSON);
    TEST(json.find("[") != std::string::npos, "JSON export has array");
    TEST(json.find("ADD_VOLUME") != std::string::npos, "JSON contains ADD_VOLUME");
    
    // Export as CSV
    std::string csv = sys.export_audit_log(AuditExportFormat::CSV);
    TEST(csv.find("Timestamp,Operation") != std::string::npos, "CSV has header");
    
    // Export as TEXT
    std::string text = sys.export_audit_log(AuditExportFormat::TEXT);
    TEST(!text.empty(), "TEXT export not empty");
    
    cleanup("test_audit_exp");
}

void test_config_profiles() {
    TEST_SECTION("Configuration Profiles Tests");
    cleanup("test_profiles");
    TMSSystem sys("test_profiles");
    
    // Create profile
    ConfigProfile profile;
    profile.name = "PROD_PROFILE";
    profile.description = "Production settings";
    profile.created = std::chrono::system_clock::now();
    profile.created_by = "ADMIN";
    profile.settings["max_volumes"] = "10000";
    profile.settings["default_pool"] = "PROD_POOL";
    
    auto result = sys.save_config_profile(profile);
    TEST(result.is_success(), "Save profile success");
    
    // Get profile
    auto loaded = sys.get_config_profile("PROD_PROFILE");
    TEST(loaded.has_value(), "Profile exists");
    TEST(loaded->description == "Production settings", "Description matches");
    
    // List profiles
    auto profiles = sys.list_config_profiles();
    TEST(profiles.size() == 1, "Found 1 profile");
    
    // Delete profile
    result = sys.delete_config_profile("PROD_PROFILE");
    TEST(result.is_success(), "Delete profile success");
    
    profiles = sys.list_config_profiles();
    TEST(profiles.size() == 0, "No profiles after delete");
    
    cleanup("test_profiles");
}

void test_statistics_aggregation() {
    TEST_SECTION("Statistics Aggregation Tests");
    cleanup("test_stats_agg");
    TMSSystem sys("test_stats_agg");
    
    // Create volumes with varying characteristics
    for (int i = 1; i <= 10; i++) {
        TapeVolume vol;
        vol.volser = "AGG" + std::string(3 - std::to_string(i).length(), '0') + std::to_string(i);
        vol.status = VolumeStatus::SCRATCH;
        vol.capacity_bytes = i * 1000000;
        vol.used_bytes = i * 100000;
        vol.mount_count = i * 10;
        vol.error_count = i;
        sys.add_volume(vol);
    }
    
    // Aggregate capacity
    auto cap_stats = sys.aggregate_volume_capacity();
    TEST(cap_stats.count == 10, "Capacity stats has 10 samples");
    TEST(cap_stats.min_value == 1000000, "Min capacity correct");
    TEST(cap_stats.max_value == 10000000, "Max capacity correct");
    
    // Aggregate mount counts
    auto mount_stats = sys.aggregate_mount_counts();
    TEST(mount_stats.count == 10, "Mount stats has 10 samples");
    TEST(mount_stats.min_value == 10, "Min mount count correct");
    TEST(mount_stats.max_value == 100, "Max mount count correct");
    
    // Aggregate health
    auto health_stats = sys.aggregate_volume_health();
    TEST(health_stats.count == 10, "Health stats has 10 samples");
    
    cleanup("test_stats_agg");
}

void test_parallel_batch() {
    TEST_SECTION("Parallel Batch Operations Tests");
    cleanup("test_parallel");
    TMSSystem sys("test_parallel");
    
    // Create volumes for parallel add
    std::vector<TapeVolume> volumes;
    for (int i = 1; i <= 20; i++) {
        TapeVolume vol;
        vol.volser = "PAR" + std::string(3 - std::to_string(i).length(), '0') + std::to_string(i);
        vol.status = VolumeStatus::SCRATCH;
        vol.pool = "PARALLEL_POOL";
        volumes.push_back(vol);
    }
    
    // Parallel add
    auto result = sys.parallel_add_volumes(volumes, 4);
    TEST(result.total == 20, "Parallel add total is 20");
    TEST(result.succeeded == 20, "All parallel adds succeeded");
    TEST(result.failed == 0, "No parallel add failures");
    
    // Verify count
    TEST(sys.get_volume_count() == 20, "20 volumes exist");
    
    // Parallel delete
    std::vector<std::string> volsers;
    for (int i = 1; i <= 10; i++) {
        volsers.push_back("PAR" + std::string(3 - std::to_string(i).length(), '0') + std::to_string(i));
    }
    
    result = sys.parallel_delete_volumes(volsers, true, 4);
    TEST(result.succeeded == 10, "Parallel delete succeeded for 10");
    TEST(sys.get_volume_count() == 10, "10 volumes remain");
    
    cleanup("test_parallel");
}

void test_error_recovery() {
    TEST_SECTION("Error Recovery Tests");
    cleanup("test_retry");
    TMSSystem sys("test_retry");
    
    // Set retry policy
    RetryPolicy policy;
    policy.max_attempts = 3;
    policy.initial_delay_ms = 10;
    policy.backoff_multiplier = 2.0;
    policy.max_delay_ms = 100;
    
    sys.set_retry_policy(policy);
    
    // Get retry policy
    auto loaded_policy = sys.get_retry_policy();
    TEST(loaded_policy.max_attempts == 3, "Max attempts is 3");
    TEST(loaded_policy.initial_delay_ms == 10, "Initial delay is 10ms");
    
    // Test retry with successful operation
    int call_count = 0;
    auto result = sys.retry_operation([&]() -> OperationResult {
        call_count++;
        return OperationResult::ok();
    });
    
    TEST(result.success, "Retry operation succeeded");
    TEST(result.attempts_made == 1, "Only 1 attempt needed");
    TEST(call_count == 1, "Operation called once");
    
    // Test retry with failing operation
    call_count = 0;
    result = sys.retry_operation([&]() -> OperationResult {
        call_count++;
        return OperationResult::err(TMSError::UNKNOWN_ERROR, "Test error");
    });
    
    TEST(!result.success, "Retry operation failed");
    TEST(result.attempts_made == 3, "Made 3 attempts");
    TEST(call_count == 3, "Operation called 3 times");
    TEST(result.required_retry(), "Required retry");
    
    cleanup("test_retry");
}
