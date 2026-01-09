/**
 * @file tms_json.h
 * @brief TMS Tape Management System - JSON Serialization Support
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides JSON import/export functionality without external dependencies.
 * Uses a lightweight, standards-compliant JSON implementation.
 */

#ifndef TMS_JSON_H
#define TMS_JSON_H

#include "tms_types.h"
#include "tms_utils.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <variant>
#include <stdexcept>

namespace tms {

// ============================================================================
// JSON Value Types
// ============================================================================

class JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

/**
 * @brief Lightweight JSON value representation
 */
class JsonValue {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };
    
    JsonValue() : type_(Type::Null) {}
    JsonValue(std::nullptr_t) : type_(Type::Null) {}
    JsonValue(bool b) : type_(Type::Boolean), bool_val_(b) {}
    JsonValue(int n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(long n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(long long n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(unsigned long n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(unsigned long long n) : type_(Type::Number), num_val_(static_cast<double>(n)) {}
    JsonValue(double n) : type_(Type::Number), num_val_(n) {}
    JsonValue(const char* s) : type_(Type::String), str_val_(s) {}
    JsonValue(const std::string& s) : type_(Type::String), str_val_(s) {}
    JsonValue(const JsonArray& arr) : type_(Type::Array), arr_val_(arr) {}
    JsonValue(const JsonObject& obj) : type_(Type::Object), obj_val_(obj) {}
    
    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Boolean; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }
    
    bool as_bool() const { return bool_val_; }
    double as_number() const { return num_val_; }
    int as_int() const { return static_cast<int>(num_val_); }
    int64_t as_int64() const { return static_cast<int64_t>(num_val_); }
    uint64_t as_uint64() const { return static_cast<uint64_t>(num_val_); }
    const std::string& as_string() const { return str_val_; }
    const JsonArray& as_array() const { return arr_val_; }
    const JsonObject& as_object() const { return obj_val_; }
    JsonArray& as_array() { return arr_val_; }
    JsonObject& as_object() { return obj_val_; }
    
    // Object access
    JsonValue& operator[](const std::string& key) { return obj_val_[key]; }
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_val;
        auto it = obj_val_.find(key);
        return it != obj_val_.end() ? it->second : null_val;
    }
    
    // Array access
    JsonValue& operator[](size_t idx) { return arr_val_[idx]; }
    const JsonValue& operator[](size_t idx) const { return arr_val_[idx]; }
    
    size_t size() const {
        if (type_ == Type::Array) return arr_val_.size();
        if (type_ == Type::Object) return obj_val_.size();
        return 0;
    }
    
    bool contains(const std::string& key) const {
        return type_ == Type::Object && obj_val_.count(key) > 0;
    }
    
private:
    Type type_;
    bool bool_val_ = false;
    double num_val_ = 0.0;
    std::string str_val_;
    JsonArray arr_val_;
    JsonObject obj_val_;
};

// ============================================================================
// JSON Serializer
// ============================================================================

/**
 * @brief JSON serialization utilities
 */
class JsonSerializer {
public:
    struct Options {
        bool pretty_print;
        int indent_size;
        bool escape_unicode;
        
        Options() : pretty_print(true), indent_size(2), escape_unicode(false) {}
    };
    
    /**
     * @brief Serialize JsonValue to string
     */
    static std::string serialize(const JsonValue& value, const Options& opts = Options()) {
        std::ostringstream oss;
        serialize_value(oss, value, opts, 0);
        return oss.str();
    }
    
    /**
     * @brief Serialize to file
     */
    static bool serialize_to_file(const JsonValue& value, const std::string& path,
                                  const Options& opts = Options()) {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << serialize(value, opts);
        return true;
    }
    
    /**
     * @brief Parse JSON string to JsonValue
     */
    static JsonValue parse(const std::string& json) {
        size_t pos = 0;
        skip_whitespace(json, pos);
        return parse_value(json, pos);
    }
    
    /**
     * @brief Parse JSON file to JsonValue
     */
    static JsonValue parse_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }
    
private:
    static void serialize_value(std::ostream& os, const JsonValue& val,
                                const Options& opts, int depth) {
        switch (val.type()) {
            case JsonValue::Type::Null:
                os << "null";
                break;
            case JsonValue::Type::Boolean:
                os << (val.as_bool() ? "true" : "false");
                break;
            case JsonValue::Type::Number:
                if (val.as_number() == static_cast<int64_t>(val.as_number())) {
                    os << static_cast<int64_t>(val.as_number());
                } else {
                    os << std::fixed << std::setprecision(6) << val.as_number();
                }
                break;
            case JsonValue::Type::String:
                os << '"' << escape_string(val.as_string()) << '"';
                break;
            case JsonValue::Type::Array:
                serialize_array(os, val.as_array(), opts, depth);
                break;
            case JsonValue::Type::Object:
                serialize_object(os, val.as_object(), opts, depth);
                break;
        }
    }
    
    static void serialize_array(std::ostream& os, const JsonArray& arr,
                                const Options& opts, int depth) {
        os << '[';
        bool first = true;
        for (const auto& val : arr) {
            if (!first) os << ',';
            first = false;
            if (opts.pretty_print) {
                os << '\n' << std::string((depth + 1) * opts.indent_size, ' ');
            }
            serialize_value(os, val, opts, depth + 1);
        }
        if (opts.pretty_print && !arr.empty()) {
            os << '\n' << std::string(depth * opts.indent_size, ' ');
        }
        os << ']';
    }
    
    static void serialize_object(std::ostream& os, const JsonObject& obj,
                                 const Options& opts, int depth) {
        os << '{';
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) os << ',';
            first = false;
            if (opts.pretty_print) {
                os << '\n' << std::string((depth + 1) * opts.indent_size, ' ');
            }
            os << '"' << escape_string(key) << '"' << ':';
            if (opts.pretty_print) os << ' ';
            serialize_value(os, val, opts, depth + 1);
        }
        if (opts.pretty_print && !obj.empty()) {
            os << '\n' << std::string(depth * opts.indent_size, ' ');
        }
        os << '}';
    }
    
    static std::string escape_string(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(c);
                    } else {
                        oss << c;
                    }
            }
        }
        return oss.str();
    }
    
    static void skip_whitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(json[pos])) pos++;
    }
    
    static JsonValue parse_value(const std::string& json, size_t& pos) {
        skip_whitespace(json, pos);
        if (pos >= json.size()) return JsonValue();
        
        char c = json[pos];
        if (c == 'n') return parse_null(json, pos);
        if (c == 't' || c == 'f') return parse_bool(json, pos);
        if (c == '"') return parse_string(json, pos);
        if (c == '[') return parse_array(json, pos);
        if (c == '{') return parse_object(json, pos);
        if (c == '-' || std::isdigit(c)) return parse_number(json, pos);
        
        throw std::runtime_error("Invalid JSON at position " + std::to_string(pos));
    }
    
    static JsonValue parse_null(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "null") {
            pos += 4;
            return JsonValue();
        }
        throw std::runtime_error("Expected 'null'");
    }
    
    static JsonValue parse_bool(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue(true);
        }
        if (json.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue(false);
        }
        throw std::runtime_error("Expected 'true' or 'false'");
    }
    
    static JsonValue parse_string(const std::string& json, size_t& pos) {
        pos++; // skip opening quote
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.size()) break;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        pos++;
                        if (pos + 4 <= json.size()) {
                            int code = std::stoi(json.substr(pos, 4), nullptr, 16);
                            if (code < 0x80) {
                                result += static_cast<char>(code);
                            }
                            pos += 3;
                        }
                        break;
                    }
                    default: result += json[pos];
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        pos++; // skip closing quote
        return JsonValue(result);
    }
    
    static JsonValue parse_number(const std::string& json, size_t& pos) {
        size_t start = pos;
        if (json[pos] == '-') pos++;
        while (pos < json.size() && std::isdigit(json[pos])) pos++;
        if (pos < json.size() && json[pos] == '.') {
            pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }
        return JsonValue(std::stod(json.substr(start, pos - start)));
    }
    
    static JsonValue parse_array(const std::string& json, size_t& pos) {
        pos++; // skip [
        JsonArray arr;
        skip_whitespace(json, pos);
        if (json[pos] == ']') {
            pos++;
            return JsonValue(arr);
        }
        while (true) {
            arr.push_back(parse_value(json, pos));
            skip_whitespace(json, pos);
            if (json[pos] == ']') {
                pos++;
                break;
            }
            if (json[pos] != ',') throw std::runtime_error("Expected ',' or ']'");
            pos++;
        }
        return JsonValue(arr);
    }
    
    static JsonValue parse_object(const std::string& json, size_t& pos) {
        pos++; // skip {
        JsonObject obj;
        skip_whitespace(json, pos);
        if (json[pos] == '}') {
            pos++;
            return JsonValue(obj);
        }
        while (true) {
            skip_whitespace(json, pos);
            if (json[pos] != '"') throw std::runtime_error("Expected string key");
            std::string key = parse_string(json, pos).as_string();
            skip_whitespace(json, pos);
            if (json[pos] != ':') throw std::runtime_error("Expected ':'");
            pos++;
            obj[key] = parse_value(json, pos);
            skip_whitespace(json, pos);
            if (json[pos] == '}') {
                pos++;
                break;
            }
            if (json[pos] != ',') throw std::runtime_error("Expected ',' or '}'");
            pos++;
        }
        return JsonValue(obj);
    }
};

// ============================================================================
// TMS Object Serialization
// ============================================================================

/**
 * @brief Convert TMS objects to/from JSON
 */
class TmsJsonConverter {
public:
    // Volume serialization
    static JsonValue volume_to_json(const TapeVolume& vol) {
        JsonObject obj;
        obj["volser"] = vol.volser;
        obj["status"] = volume_status_to_string(vol.status);
        obj["density"] = density_to_string(vol.density);
        obj["location"] = vol.location;
        obj["pool"] = vol.pool;
        obj["owner"] = vol.owner;
        obj["mount_count"] = vol.mount_count;
        obj["write_protected"] = vol.write_protected;
        obj["capacity_bytes"] = vol.capacity_bytes;
        obj["used_bytes"] = vol.used_bytes;
        obj["error_count"] = vol.error_count;
        obj["creation_date"] = format_time(vol.creation_date);
        obj["expiration_date"] = format_time(vol.expiration_date);
        obj["last_used"] = format_time(vol.last_used);
        obj["notes"] = vol.notes;
        obj["reserved_by"] = vol.reserved_by;
        
        JsonArray tags;
        for (const auto& tag : vol.tags) {
            tags.push_back(tag);
        }
        obj["tags"] = tags;
        
        JsonArray datasets;
        for (const auto& ds : vol.datasets) {
            datasets.push_back(ds);
        }
        obj["datasets"] = datasets;
        
        return JsonValue(obj);
    }
    
    static TapeVolume json_to_volume(const JsonValue& json) {
        TapeVolume vol;
        if (json.contains("volser")) vol.volser = json["volser"].as_string();
        if (json.contains("status")) vol.status = string_to_volume_status(json["status"].as_string());
        if (json.contains("density")) vol.density = string_to_density(json["density"].as_string());
        if (json.contains("location")) vol.location = json["location"].as_string();
        if (json.contains("pool")) vol.pool = json["pool"].as_string();
        if (json.contains("owner")) vol.owner = json["owner"].as_string();
        if (json.contains("mount_count")) vol.mount_count = json["mount_count"].as_int();
        if (json.contains("write_protected")) vol.write_protected = json["write_protected"].as_bool();
        if (json.contains("capacity_bytes")) vol.capacity_bytes = json["capacity_bytes"].as_uint64();
        if (json.contains("used_bytes")) vol.used_bytes = json["used_bytes"].as_uint64();
        if (json.contains("error_count")) vol.error_count = json["error_count"].as_int();
        if (json.contains("creation_date")) vol.creation_date = parse_time(json["creation_date"].as_string());
        if (json.contains("expiration_date")) vol.expiration_date = parse_time(json["expiration_date"].as_string());
        if (json.contains("notes")) vol.notes = json["notes"].as_string();
        if (json.contains("reserved_by")) vol.reserved_by = json["reserved_by"].as_string();
        
        if (json.contains("tags")) {
            for (size_t i = 0; i < json["tags"].size(); i++) {
                vol.tags.insert(json["tags"][i].as_string());
            }
        }
        
        if (json.contains("datasets")) {
            for (size_t i = 0; i < json["datasets"].size(); i++) {
                vol.datasets.push_back(json["datasets"][i].as_string());
            }
        }
        
        return vol;
    }
    
    // Dataset serialization
    static JsonValue dataset_to_json(const Dataset& ds) {
        JsonObject obj;
        obj["name"] = ds.name;
        obj["volser"] = ds.volser;
        obj["status"] = dataset_status_to_string(ds.status);
        obj["size_bytes"] = ds.size_bytes;
        obj["owner"] = ds.owner;
        obj["job_name"] = ds.job_name;
        obj["file_sequence"] = ds.file_sequence;
        obj["generation"] = ds.generation;
        obj["version"] = ds.version;
        obj["record_format"] = ds.record_format;
        obj["block_size"] = ds.block_size;
        obj["record_length"] = ds.record_length;
        obj["creation_date"] = format_time(ds.creation_date);
        obj["expiration_date"] = format_time(ds.expiration_date);
        obj["last_accessed"] = format_time(ds.last_accessed);
        obj["notes"] = ds.notes;
        
        JsonArray tags;
        for (const auto& tag : ds.tags) {
            tags.push_back(tag);
        }
        obj["tags"] = tags;
        
        return JsonValue(obj);
    }
    
    static Dataset json_to_dataset(const JsonValue& json) {
        Dataset ds;
        if (json.contains("name")) ds.name = json["name"].as_string();
        if (json.contains("volser")) ds.volser = json["volser"].as_string();
        if (json.contains("status")) ds.status = string_to_dataset_status(json["status"].as_string());
        if (json.contains("size_bytes")) ds.size_bytes = json["size_bytes"].as_uint64();
        if (json.contains("owner")) ds.owner = json["owner"].as_string();
        if (json.contains("job_name")) ds.job_name = json["job_name"].as_string();
        if (json.contains("file_sequence")) ds.file_sequence = json["file_sequence"].as_int();
        if (json.contains("generation")) ds.generation = json["generation"].as_int();
        if (json.contains("version")) ds.version = json["version"].as_int();
        if (json.contains("record_format")) ds.record_format = json["record_format"].as_string();
        if (json.contains("block_size")) ds.block_size = json["block_size"].as_uint64();
        if (json.contains("record_length")) ds.record_length = json["record_length"].as_uint64();
        if (json.contains("creation_date")) ds.creation_date = parse_time(json["creation_date"].as_string());
        if (json.contains("expiration_date")) ds.expiration_date = parse_time(json["expiration_date"].as_string());
        if (json.contains("notes")) ds.notes = json["notes"].as_string();
        
        if (json.contains("tags")) {
            for (size_t i = 0; i < json["tags"].size(); i++) {
                ds.tags.insert(json["tags"][i].as_string());
            }
        }
        
        return ds;
    }
    
    // Catalog serialization
    static JsonValue catalog_to_json(const std::vector<TapeVolume>& volumes,
                                     const std::vector<Dataset>& datasets) {
        JsonObject catalog;
        catalog["version"] = "3.3.0";
        catalog["generated"] = get_timestamp();
        
        JsonArray vol_arr;
        for (const auto& vol : volumes) {
            vol_arr.push_back(volume_to_json(vol));
        }
        catalog["volumes"] = vol_arr;
        
        JsonArray ds_arr;
        for (const auto& ds : datasets) {
            ds_arr.push_back(dataset_to_json(ds));
        }
        catalog["datasets"] = ds_arr;
        
        return JsonValue(catalog);
    }
    
    // Statistics serialization
    static JsonValue statistics_to_json(const SystemStatistics& stats) {
        JsonObject obj;
        obj["total_volumes"] = stats.total_volumes;
        obj["scratch_volumes"] = stats.scratch_volumes;
        obj["private_volumes"] = stats.private_volumes;
        obj["mounted_volumes"] = stats.mounted_volumes;
        obj["expired_volumes"] = stats.expired_volumes;
        obj["reserved_volumes"] = stats.reserved_volumes;
        obj["total_datasets"] = stats.total_datasets;
        obj["active_datasets"] = stats.active_datasets;
        obj["migrated_datasets"] = stats.migrated_datasets;
        obj["expired_datasets"] = stats.expired_datasets;
        obj["total_capacity"] = stats.total_capacity;
        obj["used_capacity"] = stats.used_capacity;
        obj["utilization_percent"] = stats.get_utilization();
        obj["uptime"] = stats.get_uptime();
        obj["operations_count"] = stats.operations_count;
        
        JsonObject pools;
        for (const auto& [name, count] : stats.pool_counts) {
            pools[name] = count;
        }
        obj["pool_counts"] = pools;
        
        return JsonValue(obj);
    }
};

} // namespace tms

#endif // TMS_JSON_H
