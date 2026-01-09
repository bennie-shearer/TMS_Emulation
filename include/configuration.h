/**
 * @file configuration.h
 * @brief TMS Tape Management System - Configuration Management
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#ifndef TMS_CONFIGURATION_H
#define TMS_CONFIGURATION_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <functional>
#include <optional>

namespace tms {

/**
 * @brief Configuration management with INI file support
 */
class Configuration {
public:
    using ChangeCallback = std::function<void(const std::string&, const std::string&)>;
    
    static Configuration& instance() {
        static Configuration config;
        return config;
    }
    
    // File operations
    bool load_from_file(const std::string& path);
    bool save_to_file(const std::string& path) const;
    bool reload();
    std::string get_config_path() const { return config_path_; }
    
    // Getters - General
    std::string get_data_directory() const;
    size_t get_max_volumes() const;
    size_t get_max_datasets() const;
    bool get_auto_save() const;
    int get_auto_save_interval() const;
    bool get_strict_validation() const;
    
    // Getters - Catalog
    bool get_enable_compression() const;
    bool get_enable_backup() const;
    int get_backup_retention_days() const;
    std::string get_backup_directory() const;
    
    // Getters - Logging
    std::string get_log_level() const;
    bool get_log_to_file() const;
    std::string get_log_file() const;
    bool get_log_to_console() const;
    size_t get_log_max_size() const;
    size_t get_log_max_files() const;
    
    // Getters - Audit
    bool get_enable_audit() const;
    int get_audit_retention_days() const;
    std::string get_audit_file() const;
    
    // Getters - Performance
    int get_lock_timeout_ms() const;
    int get_retry_count() const;
    int get_retry_delay_ms() const;
    size_t get_batch_size() const;
    
    // Generic getters
    std::string get_string(const std::string& section, const std::string& key,
                          const std::string& default_val = "") const;
    int get_int(const std::string& section, const std::string& key, int default_val = 0) const;
    bool get_bool(const std::string& section, const std::string& key, bool default_val = false) const;
    size_t get_size(const std::string& section, const std::string& key, size_t default_val = 0) const;
    
    // Setters
    void set_data_directory(const std::string& dir);
    void set_log_level(const std::string& level);
    void set_string(const std::string& section, const std::string& key, const std::string& value);
    void set_int(const std::string& section, const std::string& key, int value);
    void set_bool(const std::string& section, const std::string& key, bool value);
    
    // Section operations
    std::vector<std::string> get_sections() const;
    std::vector<std::string> get_keys(const std::string& section) const;
    bool has_section(const std::string& section) const;
    bool has_key(const std::string& section, const std::string& key) const;
    void remove_key(const std::string& section, const std::string& key);
    void remove_section(const std::string& section);
    
    // Validation
    std::vector<std::string> validate() const;
    bool is_valid() const { return validate().empty(); }
    
    // Change notification
    void register_callback(const std::string& key, ChangeCallback callback);
    void unregister_callbacks(const std::string& key);
    
    // Utility
    std::string to_string() const;
    void set_defaults();
    void merge_from(const Configuration& other);
    
private:
    Configuration();
    Configuration(const Configuration&) = delete;
    Configuration& operator=(const Configuration&) = delete;
    
    std::string expand_env_vars(const std::string& value) const;
    void notify_change(const std::string& key, const std::string& value);
    
    mutable std::mutex mutex_;
    std::map<std::string, std::map<std::string, std::string>> sections_;
    std::string config_path_;
    std::map<std::string, std::vector<ChangeCallback>> callbacks_;
};

} // namespace tms

#endif // TMS_CONFIGURATION_H
