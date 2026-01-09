/**
 * @file basic_usage.cpp
 * @brief TMS Tape Management System - Usage Example
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * This example demonstrates basic usage of the TMS Tape Management System
 * including volume management, dataset operations, and system reporting.
 */

#include "tms_tape_mgmt.h"
#include <iostream>

using namespace tms;

int main() {
    std::cout << "\n";
    std::cout << "================================================\n";
    std::cout << "  TMS Basic Usage Example v" << VERSION_STRING << "\n";
    std::cout << "================================================\n\n";
    
    // Initialize the TMS system with a data directory
    TMSSystem system("example_data");
    std::cout << "[*] TMS System initialized\n";
    std::cout << "    Data directory: " << system.get_data_directory() << "\n\n";
    
    // ========================================================================
    // Volume Management
    // ========================================================================
    std::cout << "--- Volume Management ---\n\n";
    
    // Create a new tape volume
    TapeVolume volume;
    volume.volser = "VOL001";
    volume.status = VolumeStatus::SCRATCH;
    volume.density = TapeDensity::DENSITY_LTO3;
    volume.location = "SLOT-A01";
    volume.owner = "ADMIN";
    volume.pool = "SCRATCH";
    volume.capacity_bytes = get_density_capacity(TapeDensity::DENSITY_LTO3);
    
    auto result = system.add_volume(volume);
    if (result.is_success()) {
        std::cout << "[OK] Volume VOL001 added successfully\n";
        std::cout << "    Capacity: " << format_bytes(volume.capacity_bytes) << "\n";
    } else {
        std::cout << "[INFO] " << result.error().message << "\n";
    }
    
    // Add more volumes for the pool
    for (int i = 2; i <= 5; i++) {
        TapeVolume v;
        v.volser = "VOL00" + std::to_string(i);
        v.status = VolumeStatus::SCRATCH;
        v.density = TapeDensity::DENSITY_LTO3;
        v.location = "SLOT-A0" + std::to_string(i);
        v.owner = "ADMIN";
        v.pool = "SCRATCH";
        v.capacity_bytes = get_density_capacity(v.density);
        system.add_volume(v);
    }
    std::cout << "[OK] Added 4 more volumes to pool\n\n";
    
    // ========================================================================
    // Dataset Management
    // ========================================================================
    std::cout << "--- Dataset Management ---\n\n";
    
    // Create a dataset on the volume
    Dataset dataset;
    dataset.name = "PROD.DATA.BACKUP";
    dataset.volser = "VOL001";
    dataset.size_bytes = 1024ULL * 1024 * 1024;  // 1 GB
    dataset.owner = "BACKUP";
    dataset.job_name = "BKUPJOB";
    dataset.record_format = "VB";
    dataset.block_size = 32760;
    
    auto ds_result = system.add_dataset(dataset);
    if (ds_result.is_success()) {
        std::cout << "[OK] Dataset PROD.DATA.BACKUP added\n";
        std::cout << "    Size: " << format_bytes(dataset.size_bytes) << "\n";
        std::cout << "    Volume: " << dataset.volser << "\n";
    } else {
        std::cout << "[INFO] " << ds_result.error().message << "\n";
    }
    
    // Check volume status changed
    auto vol_check = system.get_volume("VOL001");
    if (vol_check) {
        std::cout << "[*] Volume VOL001 status: " 
                  << volume_status_to_string(vol_check.value().status) << "\n\n";
    }
    
    // ========================================================================
    // Tape Operations
    // ========================================================================
    std::cout << "--- Tape Operations ---\n\n";
    
    // Mount volume
    auto mount_result = system.mount_volume("VOL001");
    std::cout << "[*] Mount VOL001: " 
              << (mount_result ? "Success" : mount_result.error().message) << "\n";
    
    // Dismount volume
    auto dismount_result = system.dismount_volume("VOL001");
    std::cout << "[*] Dismount VOL001: " 
              << (dismount_result ? "Success" : dismount_result.error().message) << "\n\n";
    
    // ========================================================================
    // Scratch Pool Management
    // ========================================================================
    std::cout << "--- Scratch Pool Management ---\n\n";
    
    auto [available, total] = system.get_scratch_pool_stats();
    std::cout << "[*] Scratch pool: " << available << "/" << total << " available\n";
    
    // Allocate a scratch volume
    auto alloc_result = system.allocate_scratch_volume();
    if (alloc_result) {
        std::cout << "[OK] Allocated scratch volume: " << alloc_result.value() << "\n";
    }
    
    // Pool statistics
    auto pool_stats = system.get_pool_statistics("SCRATCH");
    std::cout << "[*] Pool 'SCRATCH' statistics:\n";
    std::cout << "    Total volumes: " << pool_stats.total_volumes << "\n";
    std::cout << "    Scratch: " << pool_stats.scratch_volumes << "\n";
    std::cout << "    Private: " << pool_stats.private_volumes << "\n\n";
    
    // ========================================================================
    // Tagging
    // ========================================================================
    std::cout << "--- Tagging ---\n\n";
    
    system.add_volume_tag("VOL001", "production");
    system.add_volume_tag("VOL001", "backup");
    std::cout << "[OK] Added tags to VOL001\n";
    
    auto tagged_vols = system.find_volumes_by_tag("production");
    std::cout << "[*] Volumes with 'production' tag: " << tagged_vols.size() << "\n\n";
    
    // ========================================================================
    // Reports and Statistics
    // ========================================================================
    std::cout << "--- Reports ---\n";
    
    // Generate statistics
    system.generate_statistics(std::cout);
    
    // System health check
    std::cout << "\n--- Health Check ---\n\n";
    auto health = system.perform_health_check();
    std::cout << "[*] System health: " << (health.healthy ? "HEALTHY" : "ISSUES DETECTED") << "\n";
    for (const auto& [key, value] : health.metrics) {
        std::cout << "    " << key << ": " << value << "\n";
    }
    
    // ========================================================================
    // Persistence
    // ========================================================================
    std::cout << "\n--- Persistence ---\n\n";
    
    auto save_result = system.save_catalog();
    std::cout << "[*] Save catalog: " 
              << (save_result ? "Success" : save_result.error().message) << "\n";
    
    auto backup_result = system.backup_catalog();
    std::cout << "[*] Backup catalog: " 
              << (backup_result ? "Success" : backup_result.error().message) << "\n";
    
    // ========================================================================
    // Audit Log
    // ========================================================================
    std::cout << "\n--- Recent Audit Log ---\n\n";
    
    auto audit = system.get_audit_log(5);
    for (const auto& rec : audit) {
        std::cout << "  " << format_time(rec.timestamp) << " "
                  << rec.operation << " " << rec.target << "\n";
    }
    
    std::cout << "\n================================================\n";
    std::cout << "  Example completed successfully!\n";
    std::cout << "================================================\n\n";
    
    return 0;
}
