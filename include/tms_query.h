/**
 * @file tms_query.h
 * @brief TMS Tape Management System - Enhanced Query Language
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides an enhanced query language with boolean operators,
 * field-specific searches, and saved query support.
 */

#ifndef TMS_QUERY_H
#define TMS_QUERY_H

#include "tms_types.h"
#include "tms_utils.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <sstream>
#include <regex>

namespace tms {

// ============================================================================
// Query Types
// ============================================================================

/**
 * @brief Query operator types
 */
enum class QueryOperator {
    AND,
    OR,
    NOT,
    EQUALS,
    NOT_EQUALS,
    CONTAINS,
    STARTS_WITH,
    ENDS_WITH,
    GREATER_THAN,
    LESS_THAN,
    GREATER_EQUALS,
    LESS_EQUALS,
    BETWEEN,
    IN,
    MATCHES     // Regex
};

/**
 * @brief Query target field
 */
enum class QueryField {
    // Volume fields
    VOLSER,
    VOLUME_STATUS,
    VOLUME_DENSITY,
    VOLUME_LOCATION,
    VOLUME_POOL,
    VOLUME_OWNER,
    VOLUME_CAPACITY,
    VOLUME_USED,
    VOLUME_MOUNT_COUNT,
    VOLUME_CREATED,
    VOLUME_EXPIRES,
    VOLUME_TAG,
    
    // Dataset fields
    DATASET_NAME,
    DATASET_VOLSER,
    DATASET_STATUS,
    DATASET_SIZE,
    DATASET_OWNER,
    DATASET_CREATED,
    DATASET_EXPIRES,
    DATASET_TAG,
    
    // Any field
    ANY
};

/**
 * @brief Query condition
 */
struct QueryCondition {
    QueryField field = QueryField::ANY;
    QueryOperator op = QueryOperator::CONTAINS;
    std::string value;
    std::string value2;  // For BETWEEN operator
    std::vector<std::string> values;  // For IN operator
    
    QueryCondition() = default;
    QueryCondition(QueryField f, QueryOperator o, const std::string& v)
        : field(f), op(o), value(v) {}
};

/**
 * @brief Query expression node
 */
class QueryExpression {
public:
    virtual ~QueryExpression() = default;
    virtual bool matches_volume(const TapeVolume& vol) const = 0;
    virtual bool matches_dataset(const Dataset& ds) const = 0;
    virtual std::string to_string() const = 0;
};

/**
 * @brief Saved query
 */
struct SavedQuery {
    std::string name;
    std::string description;
    std::string query_string;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point last_used;
    size_t use_count = 0;
};

// ============================================================================
// Query Builder
// ============================================================================

/**
 * @brief Fluent query builder
 */
class QueryBuilder {
public:
    QueryBuilder() = default;
    
    // Field selection
    QueryBuilder& field(QueryField f) { 
        current_field_ = f; 
        return *this; 
    }
    
    QueryBuilder& volser() { return field(QueryField::VOLSER); }
    QueryBuilder& status() { return field(QueryField::VOLUME_STATUS); }
    QueryBuilder& owner() { return field(QueryField::VOLUME_OWNER); }
    QueryBuilder& pool() { return field(QueryField::VOLUME_POOL); }
    QueryBuilder& location() { return field(QueryField::VOLUME_LOCATION); }
    QueryBuilder& tag() { return field(QueryField::VOLUME_TAG); }
    
    // Operators
    QueryBuilder& equals(const std::string& val);
    QueryBuilder& not_equals(const std::string& val);
    QueryBuilder& contains(const std::string& val);
    QueryBuilder& starts_with(const std::string& val);
    QueryBuilder& ends_with(const std::string& val);
    QueryBuilder& greater_than(const std::string& val);
    QueryBuilder& less_than(const std::string& val);
    QueryBuilder& between(const std::string& low, const std::string& high);
    QueryBuilder& in_list(const std::vector<std::string>& vals);
    QueryBuilder& matches(const std::string& pattern);
    
    // Boolean combinators
    QueryBuilder& and_();
    QueryBuilder& or_();
    QueryBuilder& not_();
    
    // Build
    std::vector<QueryCondition> build() const { return conditions_; }
    std::string to_query_string() const;
    
    // Reset
    void clear() { conditions_.clear(); }
    
private:
    std::vector<QueryCondition> conditions_;
    QueryField current_field_ = QueryField::ANY;
    QueryOperator pending_logic_ = QueryOperator::AND;
};

// ============================================================================
// Query Engine
// ============================================================================

/**
 * @brief Query execution engine
 */
class QueryEngine {
public:
    using VolumeListCallback = std::function<std::vector<TapeVolume>()>;
    using DatasetListCallback = std::function<std::vector<Dataset>()>;
    
    QueryEngine() = default;
    
    // Query execution
    std::vector<TapeVolume> query_volumes(
        const std::string& query_string,
        VolumeListCallback get_volumes);
    
    std::vector<TapeVolume> query_volumes(
        const std::vector<QueryCondition>& conditions,
        VolumeListCallback get_volumes);
    
    std::vector<Dataset> query_datasets(
        const std::string& query_string,
        DatasetListCallback get_datasets);
    
    std::vector<Dataset> query_datasets(
        const std::vector<QueryCondition>& conditions,
        DatasetListCallback get_datasets);
    
    // Query parsing
    std::vector<QueryCondition> parse_query(const std::string& query_string);
    
    // Saved queries
    OperationResult save_query(const SavedQuery& query);
    OperationResult delete_query(const std::string& name);
    std::optional<SavedQuery> get_query(const std::string& name);
    std::vector<SavedQuery> list_saved_queries() const;
    
    // Query validation
    bool validate_query(const std::string& query_string, std::string& error);
    
    // Query help
    static std::string get_query_syntax_help();
    static std::vector<std::string> get_field_names();
    static std::vector<std::string> get_operator_names();
    
private:
    bool evaluate_condition(const QueryCondition& cond, const TapeVolume& vol) const;
    bool evaluate_condition(const QueryCondition& cond, const Dataset& ds) const;
    std::string get_volume_field_value(const TapeVolume& vol, QueryField field) const;
    std::string get_dataset_field_value(const Dataset& ds, QueryField field) const;
    bool compare_values(const std::string& actual, const std::string& expected, 
                        QueryOperator op) const;
    
    std::map<std::string, SavedQuery> saved_queries_;
};

// ============================================================================
// Implementation
// ============================================================================

inline QueryBuilder& QueryBuilder::equals(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::EQUALS, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::not_equals(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::NOT_EQUALS, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::contains(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::CONTAINS, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::starts_with(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::STARTS_WITH, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::ends_with(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::ENDS_WITH, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::greater_than(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::GREATER_THAN, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::less_than(const std::string& val) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::LESS_THAN, val));
    return *this;
}

inline QueryBuilder& QueryBuilder::between(const std::string& low, const std::string& high) {
    QueryCondition cond(current_field_, QueryOperator::BETWEEN, low);
    cond.value2 = high;
    conditions_.push_back(cond);
    return *this;
}

inline QueryBuilder& QueryBuilder::in_list(const std::vector<std::string>& vals) {
    QueryCondition cond(current_field_, QueryOperator::IN, "");
    cond.values = vals;
    conditions_.push_back(cond);
    return *this;
}

inline QueryBuilder& QueryBuilder::matches(const std::string& pattern) {
    conditions_.push_back(QueryCondition(current_field_, QueryOperator::MATCHES, pattern));
    return *this;
}

inline std::vector<TapeVolume> QueryEngine::query_volumes(
    const std::vector<QueryCondition>& conditions,
    VolumeListCallback get_volumes) {
    
    std::vector<TapeVolume> result;
    auto volumes = get_volumes();
    
    for (const auto& vol : volumes) {
        bool match = true;
        for (const auto& cond : conditions) {
            if (!evaluate_condition(cond, vol)) {
                match = false;
                break;
            }
        }
        if (match) {
            result.push_back(vol);
        }
    }
    
    return result;
}

inline std::vector<TapeVolume> QueryEngine::query_volumes(
    const std::string& query_string,
    VolumeListCallback get_volumes) {
    
    auto conditions = parse_query(query_string);
    return query_volumes(conditions, get_volumes);
}

inline std::vector<Dataset> QueryEngine::query_datasets(
    const std::vector<QueryCondition>& conditions,
    DatasetListCallback get_datasets) {
    
    std::vector<Dataset> result;
    auto datasets = get_datasets();
    
    for (const auto& ds : datasets) {
        bool match = true;
        for (const auto& cond : conditions) {
            if (!evaluate_condition(cond, ds)) {
                match = false;
                break;
            }
        }
        if (match) {
            result.push_back(ds);
        }
    }
    
    return result;
}

inline std::vector<QueryCondition> QueryEngine::parse_query(const std::string& query_string) {
    std::vector<QueryCondition> conditions;
    
    // Simple parser: field:operator:value format
    // Examples: "owner:eq:ADMIN", "pool:contains:PROD", "volser:starts:ABC"
    
    std::istringstream iss(query_string);
    std::string token;
    
    while (std::getline(iss, token, ' ')) {
        if (token.empty()) continue;
        
        // Parse field:op:value
        std::vector<std::string> parts;
        std::istringstream part_stream(token);
        std::string part;
        while (std::getline(part_stream, part, ':')) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 2) {
            QueryCondition cond;
            
            // Parse field
            std::string field = parts[0];
            if (field == "volser") cond.field = QueryField::VOLSER;
            else if (field == "status") cond.field = QueryField::VOLUME_STATUS;
            else if (field == "owner") cond.field = QueryField::VOLUME_OWNER;
            else if (field == "pool") cond.field = QueryField::VOLUME_POOL;
            else if (field == "location") cond.field = QueryField::VOLUME_LOCATION;
            else if (field == "tag") cond.field = QueryField::VOLUME_TAG;
            else if (field == "name") cond.field = QueryField::DATASET_NAME;
            else cond.field = QueryField::ANY;
            
            // Parse operator
            std::string op = parts.size() >= 3 ? parts[1] : "eq";
            if (op == "eq") cond.op = QueryOperator::EQUALS;
            else if (op == "ne") cond.op = QueryOperator::NOT_EQUALS;
            else if (op == "contains") cond.op = QueryOperator::CONTAINS;
            else if (op == "starts") cond.op = QueryOperator::STARTS_WITH;
            else if (op == "ends") cond.op = QueryOperator::ENDS_WITH;
            else if (op == "gt") cond.op = QueryOperator::GREATER_THAN;
            else if (op == "lt") cond.op = QueryOperator::LESS_THAN;
            else if (op == "regex") cond.op = QueryOperator::MATCHES;
            else cond.op = QueryOperator::CONTAINS;
            
            // Parse value
            cond.value = parts.size() >= 3 ? parts[2] : parts[1];
            
            conditions.push_back(cond);
        } else if (parts.size() == 1) {
            // Simple text search across all fields
            QueryCondition cond;
            cond.field = QueryField::ANY;
            cond.op = QueryOperator::CONTAINS;
            cond.value = parts[0];
            conditions.push_back(cond);
        }
    }
    
    return conditions;
}

inline bool QueryEngine::evaluate_condition(const QueryCondition& cond, const TapeVolume& vol) const {
    std::string value = get_volume_field_value(vol, cond.field);
    return compare_values(value, cond.value, cond.op);
}

inline bool QueryEngine::evaluate_condition(const QueryCondition& cond, const Dataset& ds) const {
    std::string value = get_dataset_field_value(ds, cond.field);
    return compare_values(value, cond.value, cond.op);
}

inline std::string QueryEngine::get_volume_field_value(const TapeVolume& vol, QueryField field) const {
    switch (field) {
        case QueryField::VOLSER: return vol.volser;
        case QueryField::VOLUME_STATUS: return volume_status_to_string(vol.status);
        case QueryField::VOLUME_DENSITY: return density_to_string(vol.density);
        case QueryField::VOLUME_LOCATION: return vol.location;
        case QueryField::VOLUME_POOL: return vol.pool;
        case QueryField::VOLUME_OWNER: return vol.owner;
        case QueryField::VOLUME_CAPACITY: return std::to_string(vol.capacity_bytes);
        case QueryField::VOLUME_USED: return std::to_string(vol.used_bytes);
        case QueryField::VOLUME_MOUNT_COUNT: return std::to_string(vol.mount_count);
        case QueryField::ANY:
            // Concatenate searchable fields
            return vol.volser + " " + vol.owner + " " + vol.pool + " " + vol.location;
        default: return "";
    }
}

inline std::string QueryEngine::get_dataset_field_value(const Dataset& ds, QueryField field) const {
    switch (field) {
        case QueryField::DATASET_NAME: return ds.name;
        case QueryField::DATASET_VOLSER: return ds.volser;
        case QueryField::DATASET_STATUS: return dataset_status_to_string(ds.status);
        case QueryField::DATASET_SIZE: return std::to_string(ds.size_bytes);
        case QueryField::DATASET_OWNER: return ds.owner;
        case QueryField::ANY:
            return ds.name + " " + ds.volser + " " + ds.owner;
        default: return "";
    }
}

inline bool QueryEngine::compare_values(const std::string& actual, const std::string& expected,
                                         QueryOperator op) const {
    std::string act_upper = to_upper(actual);
    std::string exp_upper = to_upper(expected);
    
    switch (op) {
        case QueryOperator::EQUALS:
            return act_upper == exp_upper;
        case QueryOperator::NOT_EQUALS:
            return act_upper != exp_upper;
        case QueryOperator::CONTAINS:
            return act_upper.find(exp_upper) != std::string::npos;
        case QueryOperator::STARTS_WITH:
            return act_upper.find(exp_upper) == 0;
        case QueryOperator::ENDS_WITH:
            return act_upper.length() >= exp_upper.length() &&
                   act_upper.substr(act_upper.length() - exp_upper.length()) == exp_upper;
        case QueryOperator::GREATER_THAN:
            try { return std::stoll(actual) > std::stoll(expected); } catch (...) { return false; }
        case QueryOperator::LESS_THAN:
            try { return std::stoll(actual) < std::stoll(expected); } catch (...) { return false; }
        case QueryOperator::MATCHES:
            try {
                std::regex re(expected, std::regex::icase);
                return std::regex_search(actual, re);
            } catch (...) { return false; }
        default:
            return false;
    }
}

inline std::string QueryEngine::get_query_syntax_help() {
    return R"(
TMS Query Language Syntax
=========================

Simple Search:
  <text>                    Search all fields for text

Field-Specific Search:
  field:value               Field equals value
  field:op:value            Field matches value using operator

Fields:
  volser, status, owner, pool, location, tag, name

Operators:
  eq        Equals
  ne        Not equals
  contains  Contains substring
  starts    Starts with
  ends      Ends with
  gt        Greater than (numeric)
  lt        Less than (numeric)
  regex     Regular expression match

Examples:
  owner:eq:ADMIN            Volumes owned by ADMIN
  pool:contains:PROD        Pools containing "PROD"
  volser:starts:ABC         Volsers starting with ABC
  status:eq:SCRATCH         Scratch volumes
  BACKUP                    Any field containing "BACKUP"
)";
}

inline OperationResult QueryEngine::save_query(const SavedQuery& query) {
    if (query.name.empty()) {
        return OperationResult::err(TMSError::INVALID_PARAMETER, "Query name cannot be empty");
    }
    
    SavedQuery q = query;
    q.created = std::chrono::system_clock::now();
    saved_queries_[q.name] = q;
    
    return OperationResult::ok();
}

inline OperationResult QueryEngine::delete_query(const std::string& name) {
    auto it = saved_queries_.find(name);
    if (it == saved_queries_.end()) {
        return OperationResult::err(TMSError::VOLUME_NOT_FOUND, "Query not found: " + name);
    }
    saved_queries_.erase(it);
    return OperationResult::ok();
}

inline std::optional<SavedQuery> QueryEngine::get_query(const std::string& name) {
    auto it = saved_queries_.find(name);
    if (it == saved_queries_.end()) {
        return std::nullopt;
    }
    it->second.last_used = std::chrono::system_clock::now();
    it->second.use_count++;
    return it->second;
}

inline std::vector<SavedQuery> QueryEngine::list_saved_queries() const {
    std::vector<SavedQuery> result;
    for (const auto& [name, query] : saved_queries_) {
        result.push_back(query);
    }
    return result;
}

inline std::vector<std::string> QueryEngine::get_field_names() {
    return {
        "volser", "status", "density", "location", "pool", "owner",
        "capacity", "used", "mount_count", "created", "expires", "tags",
        "dataset_name", "dataset_volser", "dataset_status", "dataset_size",
        "dataset_owner", "dataset_created", "dataset_expires"
    };
}

inline std::vector<std::string> QueryEngine::get_operator_names() {
    return {
        "=", "!=", "<", ">", "<=", ">=", "LIKE", "IN", "BETWEEN",
        "CONTAINS", "STARTS_WITH", "ENDS_WITH", "MATCHES"
    };
}

} // namespace tms

#endif // TMS_QUERY_H
