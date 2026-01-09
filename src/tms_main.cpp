/**
 * @file tms_main.cpp
 * @brief TMS Tape Management System - Interactive Console Application
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#include "tms_tape_mgmt.h"
#include "logger.h"
#include "configuration.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>

using namespace tms;

void print_banner() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  TMS TAPE MANAGEMENT SYSTEM EMULATOR v" << VERSION_STRING << "\n";
    std::cout << "  Cross-platform: Windows, Linux, macOS\n";
    std::cout << "  " << VERSION_COPYRIGHT << "\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
}

void print_menu() {
    std::cout << "\n--- MAIN MENU ---\n";
    std::cout << "\nVOLUME OPERATIONS:\n";
    std::cout << "  1. Add Volume           6. Dismount Volume\n";
    std::cout << "  2. List Volumes         7. Scratch Volume\n";
    std::cout << "  3. Volume Details       8. Allocate Scratch\n";
    std::cout << "  4. Mount Volume         9. Delete Volume\n";
    std::cout << "  5. Search Volumes      10. Reserve Volume\n";
    std::cout << "\nDATASET OPERATIONS:\n";
    std::cout << " 11. Add Dataset         14. Delete Dataset\n";
    std::cout << " 12. List Datasets       15. Recall Dataset\n";
    std::cout << " 13. Dataset Details     16. Migrate Dataset\n";
    std::cout << "\nREPORTS & UTILITIES:\n";
    std::cout << " 17. Volume Report       20. Export CSV\n";
    std::cout << " 18. Dataset Report      21. Pool Report\n";
    std::cout << " 19. Statistics          22. Expiration Report\n";
    std::cout << "\nSYSTEM:\n";
    std::cout << " 23. Process Expirations 26. Health Check\n";
    std::cout << " 24. Save Catalog        27. View Audit Log\n";
    std::cout << " 25. Backup Catalog      28. Configuration\n";
    std::cout << "  0. Exit\n";
    std::cout << "\nEnter choice: ";
}

void add_volume_interactive(TMSSystem& system) {
    std::string volser, location, owner, pool;
    
    std::cout << "\n--- ADD NEW VOLUME ---\n";
    std::cout << "Volume serial (1-6 chars): ";
    std::cin >> volser;
    volser = to_upper(volser);
    
    std::cin.ignore();
    std::cout << "Location: ";
    std::getline(std::cin, location);
    
    std::cout << "Owner: ";
    std::getline(std::cin, owner);
    
    std::cout << "Pool (Enter for none): ";
    std::getline(std::cin, pool);
    
    std::cout << "\nDensity: 1=LTO-1  2=LTO-2  3=LTO-3  4=LTO-4  5=LTO-5\n";
    std::cout << "         6=LTO-6  7=LTO-7  8=LTO-8  9=LTO-9\n";
    std::cout << "Choice [3]: ";
    
    std::string choice_str;
    std::getline(std::cin, choice_str);
    int density_choice = choice_str.empty() ? 3 : std::stoi(choice_str);
    
    TapeVolume volume;
    volume.volser = volser;
    volume.location = location;
    volume.owner = to_upper(owner);
    volume.pool = to_upper(pool);
    volume.status = VolumeStatus::SCRATCH;
    volume.creation_date = std::chrono::system_clock::now();
    volume.expiration_date = volume.creation_date + std::chrono::hours(24 * 365);
    
    switch (density_choice) {
        case 1: volume.density = TapeDensity::DENSITY_LTO1; break;
        case 2: volume.density = TapeDensity::DENSITY_LTO2; break;
        case 4: volume.density = TapeDensity::DENSITY_LTO4; break;
        case 5: volume.density = TapeDensity::DENSITY_LTO5; break;
        case 6: volume.density = TapeDensity::DENSITY_LTO6; break;
        case 7: volume.density = TapeDensity::DENSITY_LTO7; break;
        case 8: volume.density = TapeDensity::DENSITY_LTO8; break;
        case 9: volume.density = TapeDensity::DENSITY_LTO9; break;
        default: volume.density = TapeDensity::DENSITY_LTO3; break;
    }
    volume.capacity_bytes = get_density_capacity(volume.density);
    
    auto result = system.add_volume(volume);
    std::cout << (result ? "[OK] " : "[FAIL] ") << (result ? "Volume added" : result.error().message) << "\n";
}

void add_dataset_interactive(TMSSystem& system) {
    std::string name, volser, owner, job_name;
    size_t size_mb;
    
    std::cout << "\n--- ADD NEW DATASET ---\n";
    std::cin.ignore();
    std::cout << "Dataset name: ";
    std::getline(std::cin, name);
    name = to_upper(name);
    
    std::cout << "Volume serial: ";
    std::getline(std::cin, volser);
    volser = to_upper(volser);
    
    std::cout << "Owner: ";
    std::getline(std::cin, owner);
    
    std::cout << "Job name: ";
    std::getline(std::cin, job_name);
    
    std::cout << "Size (MB): ";
    std::cin >> size_mb;
    
    Dataset dataset;
    dataset.name = name;
    dataset.volser = volser;
    dataset.owner = to_upper(owner);
    dataset.job_name = to_upper(job_name);
    dataset.status = DatasetStatus::ACTIVE;
    dataset.size_bytes = size_mb * 1024 * 1024;
    dataset.file_sequence = 1;
    dataset.creation_date = std::chrono::system_clock::now();
    dataset.expiration_date = dataset.creation_date + std::chrono::hours(24 * 30);
    
    auto result = system.add_dataset(dataset);
    std::cout << (result ? "[OK] " : "[FAIL] ") << (result ? "Dataset added" : result.error().message) << "\n";
}

void search_volumes_interactive(TMSSystem& system) {
    std::cout << "\n--- SEARCH VOLUMES ---\n";
    std::cin.ignore();
    
    SearchCriteria criteria;
    std::string input;
    
    std::cout << "Pattern (Enter for all): ";
    std::getline(std::cin, input);
    criteria.pattern = to_upper(input);
    
    if (!criteria.pattern.empty()) {
        std::cout << "Search mode (1=Exact, 2=Prefix, 3=Contains, 4=Wildcard): ";
        std::getline(std::cin, input);
        int mode = input.empty() ? 3 : std::stoi(input);
        switch (mode) {
            case 1: criteria.mode = SearchMode::EXACT; break;
            case 2: criteria.mode = SearchMode::PREFIX; break;
            case 4: criteria.mode = SearchMode::WILDCARD; break;
            default: criteria.mode = SearchMode::CONTAINS; break;
        }
    }
    
    std::cout << "Owner filter (Enter for all): ";
    std::getline(std::cin, input);
    if (!input.empty()) criteria.owner = to_upper(input);
    
    std::cout << "Pool filter (Enter for all): ";
    std::getline(std::cin, input);
    if (!input.empty()) criteria.pool = to_upper(input);
    
    criteria.limit = 50;
    
    auto results = system.search_volumes(criteria);
    
    std::cout << "\nFound " << results.size() << " volumes:\n";
    std::cout << std::left << std::setw(8) << "VOLSER"
              << std::setw(10) << "STATUS"
              << std::setw(10) << "POOL"
              << std::setw(10) << "OWNER" << "\n";
    std::cout << std::string(38, '-') << "\n";
    
    for (const auto& vol : results) {
        std::cout << std::left << std::setw(8) << vol.volser
                  << std::setw(10) << volume_status_to_string(vol.status)
                  << std::setw(10) << vol.pool
                  << std::setw(10) << vol.owner << "\n";
    }
}

void reserve_volume_interactive(TMSSystem& system) {
    std::string volser, user;
    int hours;
    
    std::cout << "\n--- RESERVE VOLUME ---\n";
    std::cout << "Volume serial: ";
    std::cin >> volser;
    volser = to_upper(volser);
    
    std::cin.ignore();
    std::cout << "User name: ";
    std::getline(std::cin, user);
    
    std::cout << "Duration (hours) [1]: ";
    std::string input;
    std::getline(std::cin, input);
    hours = input.empty() ? 1 : std::stoi(input);
    
    auto result = system.reserve_volume(volser, to_upper(user), std::chrono::hours(hours));
    std::cout << (result ? "[OK] Volume reserved" : "[FAIL] " + result.error().message) << "\n";
}

void initialize_sample_data(TMSSystem& system) {
    std::cout << "Initializing sample data...\n";
    
    // Add volumes to different pools
    for (int i = 1; i <= 5; i++) {
        TapeVolume vol;
        vol.volser = "VOL" + std::to_string(100 + i);
        vol.status = VolumeStatus::SCRATCH;
        vol.density = TapeDensity::DENSITY_LTO3;
        vol.location = "SLOT " + std::to_string(i);
        vol.owner = "SYSTEM";
        vol.pool = "POOL_A";
        vol.capacity_bytes = get_density_capacity(vol.density);
        system.add_volume(vol);
    }
    
    for (int i = 1; i <= 3; i++) {
        TapeVolume vol;
        vol.volser = "BKP" + std::to_string(200 + i);
        vol.status = VolumeStatus::SCRATCH;
        vol.density = TapeDensity::DENSITY_LTO5;
        vol.location = "SLOT " + std::to_string(10 + i);
        vol.owner = "BACKUP";
        vol.pool = "BACKUP";
        vol.capacity_bytes = get_density_capacity(vol.density);
        system.add_volume(vol);
    }
    
    // Add sample datasets
    Dataset ds1;
    ds1.name = "PROD.PAYROLL.DATA";
    ds1.volser = "VOL101";
    ds1.size_bytes = 500ULL * 1024 * 1024;
    ds1.owner = "FINANCE";
    ds1.job_name = "PAYJOB01";
    system.add_dataset(ds1);
    
    Dataset ds2;
    ds2.name = "TEST.CUSTOMER.DB";
    ds2.volser = "VOL102";
    ds2.size_bytes = 1024ULL * 1024 * 1024;
    ds2.owner = "TESTTEAM";
    ds2.job_name = "TESTJOB";
    system.add_dataset(ds2);
    
    Dataset ds3;
    ds3.name = "DEV.APPLICATION.CODE";
    ds3.volser = "VOL103";
    ds3.size_bytes = 256ULL * 1024 * 1024;
    ds3.owner = "DEVTEAM";
    ds3.job_name = "DEVJOB";
    system.add_dataset(ds3);
    
    std::cout << "[OK] 8 volumes, 3 datasets created in 2 pools\n";
}

void show_health_check(TMSSystem& system) {
    std::cout << "\n--- HEALTH CHECK ---\n";
    
    auto result = system.perform_health_check();
    
    std::cout << "Status: " << (result.healthy ? "HEALTHY" : "ISSUES DETECTED") << "\n\n";
    
    if (!result.warnings.empty()) {
        std::cout << "Warnings:\n";
        for (const auto& w : result.warnings) {
            std::cout << "  [WARN] " << w << "\n";
        }
    }
    
    if (!result.errors.empty()) {
        std::cout << "Errors:\n";
        for (const auto& e : result.errors) {
            std::cout << "  [ERROR] " << e << "\n";
        }
    }
    
    std::cout << "\nMetrics:\n";
    for (const auto& [key, value] : result.metrics) {
        std::cout << "  " << key << ": " << value << "\n";
    }
}

int main(int argc, char* argv[]) {
    // Configure logging
    Logger::instance().set_level(Logger::Level::WARNING);
    Logger::instance().enable_console(false);
    
    print_banner();
    
    // Determine data directory
    std::string data_dir = Configuration::instance().get_data_directory();
    if (argc > 1) data_dir = argv[1];
    
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "Platform: " << PLATFORM_NAME << "\n";
    
    TMSSystem system(data_dir);
    
    // Offer to initialize sample data if empty
    if (system.get_volume_count() == 0) {
        std::cout << "Initialize sample data? (y/n): ";
        char c; std::cin >> c;
        if (c == 'y' || c == 'Y') initialize_sample_data(system);
    } else {
        std::cout << "Loaded " << system.get_volume_count() << " volumes, "
                  << system.get_dataset_count() << " datasets\n";
    }
    
    int choice = -1;
    while (choice != 0) {
        print_menu();
        std::cin >> choice;
        
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }
        
        std::string input;
        switch (choice) {
            case 1: add_volume_interactive(system); break;
            case 2: system.generate_volume_report(std::cout); break;
            case 3: {
                std::cout << "Volume serial: "; std::cin >> input;
                input = to_upper(input);
                auto r = system.get_volume(input);
                if (r) {
                    const auto& v = r.value();
                    std::cout << "\nVolser: " << v.volser
                              << "\nStatus: " << volume_status_to_string(v.status)
                              << "\nDensity: " << density_to_string(v.density)
                              << "\nLocation: " << v.location
                              << "\nPool: " << v.pool
                              << "\nOwner: " << v.owner
                              << "\nMounts: " << v.mount_count
                              << "\nCapacity: " << format_bytes(v.capacity_bytes)
                              << "\nUsed: " << format_bytes(v.used_bytes)
                              << " (" << std::fixed << std::setprecision(1) << v.get_usage_percent() << "%)"
                              << "\nDatasets: " << v.datasets.size()
                              << "\nCreated: " << format_time(v.creation_date)
                              << "\nExpires: " << format_time(v.expiration_date);
                    if (v.is_reserved()) {
                        std::cout << "\nReserved by: " << v.reserved_by;
                    }
                    std::cout << "\n";
                } else std::cout << "[FAIL] " << r.error().message << "\n";
                break;
            }
            case 4:
                std::cout << "Volume to mount: "; std::cin >> input;
                input = to_upper(input);
                { auto r = system.mount_volume(input);
                  std::cout << (r ? "[OK] Mounted" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 5: search_volumes_interactive(system); break;
            case 6:
                std::cout << "Volume to dismount: "; std::cin >> input;
                input = to_upper(input);
                { auto r = system.dismount_volume(input);
                  std::cout << (r ? "[OK] Dismounted" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 7:
                std::cout << "Volume to scratch: "; std::cin >> input;
                input = to_upper(input);
                { auto r = system.scratch_volume(input);
                  std::cout << (r ? "[OK] Scratched" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 8:
                { auto r = system.allocate_scratch_volume();
                  std::cout << (r ? "[OK] Allocated: " + r.value() : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 9:
                std::cout << "Volume to delete: "; std::cin >> input;
                input = to_upper(input);
                { auto r = system.delete_volume(input);
                  std::cout << (r ? "[OK] Deleted" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 10: reserve_volume_interactive(system); break;
            case 11: add_dataset_interactive(system); break;
            case 12: system.generate_dataset_report(std::cout); break;
            case 13: {
                std::cin.ignore();
                std::cout << "Dataset name: "; std::getline(std::cin, input);
                input = to_upper(input);
                auto r = system.get_dataset(input);
                if (r) {
                    const auto& d = r.value();
                    std::cout << "\nName: " << d.name
                              << "\nVolser: " << d.volser
                              << "\nStatus: " << dataset_status_to_string(d.status)
                              << "\nOwner: " << d.owner
                              << "\nJob: " << d.job_name
                              << "\nSize: " << format_bytes(d.size_bytes)
                              << "\nSequence: " << d.file_sequence
                              << "\nCreated: " << format_time(d.creation_date)
                              << "\nExpires: " << format_time(d.expiration_date) << "\n";
                } else std::cout << "[FAIL] " << r.error().message << "\n";
                break;
            }
            case 14:
                std::cin.ignore();
                std::cout << "Dataset to delete: "; std::getline(std::cin, input);
                input = to_upper(input);
                { auto r = system.delete_dataset(input);
                  std::cout << (r ? "[OK] Deleted" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 15:
                std::cin.ignore();
                std::cout << "Dataset to recall: "; std::getline(std::cin, input);
                input = to_upper(input);
                { auto r = system.recall_dataset(input);
                  std::cout << (r ? "[OK] Recalled" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 16:
                std::cin.ignore();
                std::cout << "Dataset to migrate: "; std::getline(std::cin, input);
                input = to_upper(input);
                { auto r = system.migrate_dataset(input);
                  std::cout << (r ? "[OK] Migrated" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 17: system.generate_volume_report(std::cout); break;
            case 18: system.generate_dataset_report(std::cout); break;
            case 19: system.generate_statistics(std::cout); break;
            case 20:
                { auto r = system.export_to_csv("volumes.csv", "datasets.csv");
                  std::cout << (r ? "[OK] Exported to volumes.csv and datasets.csv" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 21: system.generate_pool_report(std::cout); break;
            case 22: system.generate_expiration_report(std::cout); break;
            case 23:
                { auto n = system.process_expirations();
                  std::cout << "[OK] Processed " << n << " expirations\n"; }
                break;
            case 24:
                { auto r = system.save_catalog();
                  std::cout << (r ? "[OK] Saved" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 25:
                { auto r = system.backup_catalog();
                  std::cout << (r ? "[OK] Backed up" : "[FAIL] " + r.error().message) << "\n"; }
                break;
            case 26: show_health_check(system); break;
            case 27:
                { auto recs = system.get_audit_log(15);
                  std::cout << "\n--- RECENT AUDIT LOG ---\n";
                  for (const auto& rec : recs) {
                      std::cout << format_time(rec.timestamp) << " "
                                << std::setw(15) << rec.operation << " "
                                << std::setw(8) << rec.target << " "
                                << rec.details << "\n";
                  }
                }
                break;
            case 28: std::cout << Configuration::instance().to_string(); break;
            case 0: std::cout << "Saving and exiting...\n"; break;
            default: std::cout << "Invalid choice\n";
        }
    }
    
    std::cout << "TMS shutdown complete.\n";
    return 0;
}
