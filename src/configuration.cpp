/**
 * @file configuration.cpp
 * @brief TMS Tape Management System - Configuration Implementation
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 */

#include "configuration.h"
#include "tms_version.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace tms {

Configuration::Configuration() {
    set_defaults();
}

void Configuration::set_defaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    sections_.clear();
    
    // General settings
    sections_["General"]["data_directory"] = "tms_data";
    sections_["General"]["max_volumes"] = "100000";
    sections_["General"]["max_datasets"] = "1000000";
    sections_["General"]["auto_save"] = "true";
    sections_["General"]["auto_save_interval"] = "300";
    sections_["General"]["strict_validation"] = "true";
    
    // Catalog settings
    sections_["Catalog"]["enable_compression"] = "false";
    sections_["Catalog"]["enable_backup"] = "true";
    sections_["Catalog"]["backup_retention_days"] = "30";
    sections_["Catalog"]["backup_directory"] = "tms_data/backups";
    
    // Logging settings
    sections_["Logging"]["log_level"] = "INFO";
    sections_["Logging"]["log_to_file"] = "true";
    sections_["Logging"]["log_file"] = "tms.log";
    sections_["Logging"]["log_to_console"] = "true";
    sections_["Logging"]["log_max_size"] = "10485760";
    sections_["Logging"]["log_max_files"] = "5";
    
    // Audit settings
    sections_["Audit"]["enable_audit"] = "true";
    sections_["Audit"]["retention_days"] = "90";
    sections_["Audit"]["audit_file"] = "tms_audit.log";
    
    // Performance settings
    sections_["Performance"]["lock_timeout_ms"] = "5000";
    sections_["Performance"]["retry_count"] = "3";
    sections_["Performance"]["retry_delay_ms"] = "100";
    sections_["Performance"]["batch_size"] = "100";
}

bool Configuration::load_from_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    config_path_ = path;
    std::string current_section = "General";
    std::string line;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        // Section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }
        
        // Key=value pair
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim key
            size_t ks = key.find_first_not_of(" \t");
            size_t ke = key.find_last_not_of(" \t");
            if (ks != std::string::npos) key = key.substr(ks, ke - ks + 1);
            
            // Trim value
            size_t vs = value.find_first_not_of(" \t");
            size_t ve = value.find_last_not_of(" \t");
            if (vs != std::string::npos) value = value.substr(vs, ve - vs + 1);
            
            // Remove quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            
            sections_[current_section][key] = expand_env_vars(value);
        }
    }
    
    return true;
}

bool Configuration::save_to_file(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create parent directories if needed
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {
        // Ignore directory creation errors
    }
    
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << "# TMS Tape Management System Configuration\n";
    file << "# Version " << VERSION_STRING << "\n";
    file << "# Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
    
    for (const auto& [section, keys] : sections_) {
        file << "[" << section << "]\n";
        for (const auto& [key, value] : keys) {
            file << key << " = " << value << "\n";
        }
        file << "\n";
    }
    
    return true;
}

bool Configuration::reload() {
    if (config_path_.empty()) return false;
    return load_from_file(config_path_);
}

std::string Configuration::expand_env_vars(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;
    
    while ((pos = result.find('$', pos)) != std::string::npos) {
        size_t start = pos;
        size_t end = pos + 1;
        
        if (end < result.size() && result[end] == '{') {
            // ${VAR} syntax
            end++;
            size_t close = result.find('}', end);
            if (close != std::string::npos) {
                std::string var_name = result.substr(end, close - end);
                const char* env_value = std::getenv(var_name.c_str());
                if (env_value) {
                    result.replace(start, close - start + 1, env_value);
                    pos = start + std::strlen(env_value);
                } else {
                    pos = close + 1;
                }
            } else {
                pos++;
            }
        } else {
            // $VAR syntax
            while (end < result.size() && 
                   (std::isalnum(static_cast<unsigned char>(result[end])) || result[end] == '_')) {
                end++;
            }
            if (end > pos + 1) {
                std::string var_name = result.substr(pos + 1, end - pos - 1);
                const char* env_value = std::getenv(var_name.c_str());
                if (env_value) {
                    result.replace(start, end - start, env_value);
                    pos = start + std::strlen(env_value);
                } else {
                    pos = end;
                }
            } else {
                pos++;
            }
        }
    }
    
    return result;
}

// Generic getters
std::string Configuration::get_string(const std::string& section, const std::string& key,
                                      const std::string& default_val) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sit = sections_.find(section);
    if (sit != sections_.end()) {
        auto kit = sit->second.find(key);
        if (kit != sit->second.end()) return kit->second;
    }
    return default_val;
}

int Configuration::get_int(const std::string& section, const std::string& key, int default_val) const {
    std::string val = get_string(section, key, "");
    if (val.empty()) return default_val;
    try { return std::stoi(val); } catch (...) { return default_val; }
}

bool Configuration::get_bool(const std::string& section, const std::string& key, bool default_val) const {
    std::string val = get_string(section, key, "");
    if (val.empty()) return default_val;
    std::transform(val.begin(), val.end(), val.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return val == "true" || val == "1" || val == "yes" || val == "on";
}

size_t Configuration::get_size(const std::string& section, const std::string& key, size_t default_val) const {
    std::string val = get_string(section, key, "");
    if (val.empty()) return default_val;
    try { return std::stoull(val); } catch (...) { return default_val; }
}

// Specific getters - General
std::string Configuration::get_data_directory() const { return get_string("General", "data_directory", "tms_data"); }
size_t Configuration::get_max_volumes() const { return get_size("General", "max_volumes", 100000); }
size_t Configuration::get_max_datasets() const { return get_size("General", "max_datasets", 1000000); }
bool Configuration::get_auto_save() const { return get_bool("General", "auto_save", true); }
int Configuration::get_auto_save_interval() const { return get_int("General", "auto_save_interval", 300); }
bool Configuration::get_strict_validation() const { return get_bool("General", "strict_validation", true); }

// Specific getters - Catalog
bool Configuration::get_enable_compression() const { return get_bool("Catalog", "enable_compression", false); }
bool Configuration::get_enable_backup() const { return get_bool("Catalog", "enable_backup", true); }
int Configuration::get_backup_retention_days() const { return get_int("Catalog", "backup_retention_days", 30); }
std::string Configuration::get_backup_directory() const { return get_string("Catalog", "backup_directory", "tms_data/backups"); }

// Specific getters - Logging
std::string Configuration::get_log_level() const { return get_string("Logging", "log_level", "INFO"); }
bool Configuration::get_log_to_file() const { return get_bool("Logging", "log_to_file", true); }
std::string Configuration::get_log_file() const { return get_string("Logging", "log_file", "tms.log"); }
bool Configuration::get_log_to_console() const { return get_bool("Logging", "log_to_console", true); }
size_t Configuration::get_log_max_size() const { return get_size("Logging", "log_max_size", 10485760); }
size_t Configuration::get_log_max_files() const { return get_size("Logging", "log_max_files", 5); }

// Specific getters - Audit
bool Configuration::get_enable_audit() const { return get_bool("Audit", "enable_audit", true); }
int Configuration::get_audit_retention_days() const { return get_int("Audit", "retention_days", 90); }
std::string Configuration::get_audit_file() const { return get_string("Audit", "audit_file", "tms_audit.log"); }

// Specific getters - Performance
int Configuration::get_lock_timeout_ms() const { return get_int("Performance", "lock_timeout_ms", 5000); }
int Configuration::get_retry_count() const { return get_int("Performance", "retry_count", 3); }
int Configuration::get_retry_delay_ms() const { return get_int("Performance", "retry_delay_ms", 100); }
size_t Configuration::get_batch_size() const { return get_size("Performance", "batch_size", 100); }

// Setters
void Configuration::set_data_directory(const std::string& dir) {
    set_string("General", "data_directory", dir);
}

void Configuration::set_log_level(const std::string& level) {
    set_string("Logging", "log_level", level);
}

void Configuration::set_string(const std::string& section, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string old_value = sections_[section][key];
    sections_[section][key] = value;
    if (old_value != value) {
        notify_change(section + "." + key, value);
    }
}

void Configuration::set_int(const std::string& section, const std::string& key, int value) {
    set_string(section, key, std::to_string(value));
}

void Configuration::set_bool(const std::string& section, const std::string& key, bool value) {
    set_string(section, key, value ? "true" : "false");
}

// Section operations
std::vector<std::string> Configuration::get_sections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& [section, _] : sections_) {
        result.push_back(section);
    }
    return result;
}

std::vector<std::string> Configuration::get_keys(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    auto it = sections_.find(section);
    if (it != sections_.end()) {
        for (const auto& [key, _] : it->second) {
            result.push_back(key);
        }
    }
    return result;
}

bool Configuration::has_section(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sections_.find(section) != sections_.end();
}

bool Configuration::has_key(const std::string& section, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sit = sections_.find(section);
    if (sit == sections_.end()) return false;
    return sit->second.find(key) != sit->second.end();
}

void Configuration::remove_key(const std::string& section, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sections_.find(section);
    if (it != sections_.end()) {
        it->second.erase(key);
    }
}

void Configuration::remove_section(const std::string& section) {
    std::lock_guard<std::mutex> lock(mutex_);
    sections_.erase(section);
}

void Configuration::register_callback(const std::string& key, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_[key].push_back(callback);
}

void Configuration::unregister_callbacks(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(key);
}

void Configuration::notify_change(const std::string& key, const std::string& value) {
    // Note: mutex already held by caller
    auto it = callbacks_.find(key);
    if (it != callbacks_.end()) {
        for (const auto& cb : it->second) {
            cb(key, value);
        }
    }
}

std::vector<std::string> Configuration::validate() const {
    std::vector<std::string> errors;
    
    if (get_data_directory().empty()) {
        errors.push_back("Data directory cannot be empty");
    }
    
    if (get_max_volumes() == 0) {
        errors.push_back("Max volumes must be greater than 0");
    }
    
    if (get_max_datasets() == 0) {
        errors.push_back("Max datasets must be greater than 0");
    }
    
    std::string level = get_log_level();
    if (level != "TRACE" && level != "DEBUG" && level != "INFO" &&
        level != "WARNING" && level != "ERROR" && level != "CRITICAL" && level != "OFF") {
        errors.push_back("Invalid log level: " + level);
    }
    
    if (get_lock_timeout_ms() <= 0) {
        errors.push_back("Lock timeout must be positive");
    }
    
    if (get_retry_count() < 0) {
        errors.push_back("Retry count cannot be negative");
    }
    
    return errors;
}

std::string Configuration::to_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    oss << "\n=== TMS CONFIGURATION ===\n";
    for (const auto& [section, keys] : sections_) {
        oss << "\n[" << section << "]\n";
        for (const auto& [key, value] : keys) {
            oss << "  " << key << " = " << value << "\n";
        }
    }
    oss << "\n";
    
    return oss.str();
}

void Configuration::merge_from(const Configuration& other) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Note: This is a simplified merge - in a real implementation,
    // you'd need to handle the other's mutex as well
    for (const auto& section : other.get_sections()) {
        for (const auto& key : other.get_keys(section)) {
            sections_[section][key] = other.get_string(section, key);
        }
    }
}

} // namespace tms
