/**
 * @file tms_events.h
 * @brief TMS Tape Management System - Event/Observer System
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides an observer pattern implementation for TMS events,
 * allowing components to subscribe to and react to system events.
 */

#ifndef TMS_EVENTS_H
#define TMS_EVENTS_H

#include "tms_types.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <memory>
#include <chrono>
#include <queue>
#include <atomic>

namespace tms {

// ============================================================================
// Event Types
// ============================================================================

/**
 * @brief Event categories
 */
enum class EventType {
    // Volume events
    VOLUME_ADDED,
    VOLUME_DELETED,
    VOLUME_MOUNTED,
    VOLUME_DISMOUNTED,
    VOLUME_SCRATCHED,
    VOLUME_EXPIRED,
    VOLUME_RESERVED,
    VOLUME_RELEASED,
    VOLUME_TAGGED,
    VOLUME_UNTAGGED,
    
    // Dataset events
    DATASET_ADDED,
    DATASET_DELETED,
    DATASET_MIGRATED,
    DATASET_RECALLED,
    DATASET_EXPIRED,
    DATASET_TAGGED,
    
    // System events
    CATALOG_SAVED,
    CATALOG_LOADED,
    BACKUP_CREATED,
    BACKUP_RESTORED,
    HEALTH_CHECK_COMPLETED,
    
    // Threshold events
    SCRATCH_POOL_LOW,
    CAPACITY_HIGH,
    ERROR_THRESHOLD_EXCEEDED,
    
    // Custom events
    CUSTOM
};

/**
 * @brief Event severity levels
 */
enum class EventSeverity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * @brief Event data structure
 */
struct Event {
    EventType type;
    EventSeverity severity = EventSeverity::INFO;
    std::chrono::system_clock::time_point timestamp;
    std::string source;         ///< Source component
    std::string target;         ///< Target (volser, dataset name, etc.)
    std::string message;        ///< Human-readable message
    std::map<std::string, std::string> data;  ///< Additional data
    uint64_t sequence_number = 0;
    
    Event() : type(EventType::CUSTOM), timestamp(std::chrono::system_clock::now()) {}
    
    Event(EventType t, const std::string& src, const std::string& tgt, const std::string& msg)
        : type(t), timestamp(std::chrono::system_clock::now()),
          source(src), target(tgt), message(msg) {}
    
    /// Add data field
    Event& with_data(const std::string& key, const std::string& value) {
        data[key] = value;
        return *this;
    }
    
    /// Set severity
    Event& with_severity(EventSeverity s) {
        severity = s;
        return *this;
    }
};

/**
 * @brief Event filter for subscriptions
 */
struct EventFilter {
    std::vector<EventType> types;           ///< Filter by event types
    std::vector<EventSeverity> severities;  ///< Filter by severity
    std::string source_pattern;             ///< Filter by source (regex)
    std::string target_pattern;             ///< Filter by target (regex)
    
    bool matches(const Event& event) const;
    
    static EventFilter all() { return EventFilter{}; }
    static EventFilter for_type(EventType type) {
        EventFilter f;
        f.types.push_back(type);
        return f;
    }
    static EventFilter for_severity(EventSeverity severity) {
        EventFilter f;
        f.severities.push_back(severity);
        return f;
    }
};

// ============================================================================
// Event Handler Types
// ============================================================================

using EventHandler = std::function<void(const Event&)>;
using EventHandlerId = uint64_t;

// ============================================================================
// Event Bus
// ============================================================================

/**
 * @brief Central event bus for publish/subscribe messaging
 */
class EventBus {
public:
    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }
    
    /// Subscribe to events
    EventHandlerId subscribe(const EventFilter& filter, EventHandler handler);
    
    /// Subscribe to specific event type
    EventHandlerId subscribe(EventType type, EventHandler handler);
    
    /// Subscribe to all events
    EventHandlerId subscribe_all(EventHandler handler);
    
    /// Unsubscribe
    void unsubscribe(EventHandlerId id);
    
    /// Publish an event
    void publish(const Event& event);
    
    /// Publish with builder pattern
    void publish(EventType type, const std::string& source,
                 const std::string& target, const std::string& message);
    
    /// Get event history
    std::vector<Event> get_history(size_t count = 100) const;
    
    /// Get events by type
    std::vector<Event> get_events_by_type(EventType type, size_t count = 100) const;
    
    /// Get events since timestamp
    std::vector<Event> get_events_since(
        const std::chrono::system_clock::time_point& since) const;
    
    /// Clear history
    void clear_history();
    
    /// Configuration
    void set_max_history(size_t max) { max_history_ = max; }
    size_t get_max_history() const { return max_history_; }
    void set_async_dispatch(bool async) { async_dispatch_ = async; }
    
    /// Statistics
    size_t get_subscriber_count() const;
    size_t get_event_count() const { return event_counter_; }
    
private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    void dispatch(const Event& event);
    void add_to_history(const Event& event);
    
    struct Subscription {
        EventHandlerId id;
        EventFilter filter;
        EventHandler handler;
    };
    
    mutable std::mutex mutex_;
    std::vector<Subscription> subscriptions_;
    std::vector<Event> history_;
    size_t max_history_ = 1000;
    std::atomic<uint64_t> event_counter_{0};
    std::atomic<EventHandlerId> next_handler_id_{1};
    bool async_dispatch_ = false;
};

// ============================================================================
// Event Helpers
// ============================================================================

/**
 * @brief Convert EventType to string
 */
inline std::string event_type_to_string(EventType type) {
    switch (type) {
        case EventType::VOLUME_ADDED: return "VOLUME_ADDED";
        case EventType::VOLUME_DELETED: return "VOLUME_DELETED";
        case EventType::VOLUME_MOUNTED: return "VOLUME_MOUNTED";
        case EventType::VOLUME_DISMOUNTED: return "VOLUME_DISMOUNTED";
        case EventType::VOLUME_SCRATCHED: return "VOLUME_SCRATCHED";
        case EventType::VOLUME_EXPIRED: return "VOLUME_EXPIRED";
        case EventType::VOLUME_RESERVED: return "VOLUME_RESERVED";
        case EventType::VOLUME_RELEASED: return "VOLUME_RELEASED";
        case EventType::VOLUME_TAGGED: return "VOLUME_TAGGED";
        case EventType::VOLUME_UNTAGGED: return "VOLUME_UNTAGGED";
        case EventType::DATASET_ADDED: return "DATASET_ADDED";
        case EventType::DATASET_DELETED: return "DATASET_DELETED";
        case EventType::DATASET_MIGRATED: return "DATASET_MIGRATED";
        case EventType::DATASET_RECALLED: return "DATASET_RECALLED";
        case EventType::DATASET_EXPIRED: return "DATASET_EXPIRED";
        case EventType::DATASET_TAGGED: return "DATASET_TAGGED";
        case EventType::CATALOG_SAVED: return "CATALOG_SAVED";
        case EventType::CATALOG_LOADED: return "CATALOG_LOADED";
        case EventType::BACKUP_CREATED: return "BACKUP_CREATED";
        case EventType::BACKUP_RESTORED: return "BACKUP_RESTORED";
        case EventType::HEALTH_CHECK_COMPLETED: return "HEALTH_CHECK_COMPLETED";
        case EventType::SCRATCH_POOL_LOW: return "SCRATCH_POOL_LOW";
        case EventType::CAPACITY_HIGH: return "CAPACITY_HIGH";
        case EventType::ERROR_THRESHOLD_EXCEEDED: return "ERROR_THRESHOLD_EXCEEDED";
        case EventType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert EventSeverity to string
 */
inline std::string severity_to_string(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::INFO: return "INFO";
        case EventSeverity::WARNING: return "WARNING";
        case EventSeverity::ERROR: return "ERROR";
        case EventSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Implementation
// ============================================================================

inline bool EventFilter::matches(const Event& event) const {
    // Check types
    if (!types.empty()) {
        bool found = false;
        for (auto t : types) {
            if (t == event.type) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check severities
    if (!severities.empty()) {
        bool found = false;
        for (auto s : severities) {
            if (s == event.severity) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check source pattern
    if (!source_pattern.empty()) {
        try {
            std::regex re(source_pattern, std::regex::icase);
            if (!std::regex_search(event.source, re)) return false;
        } catch (...) {
            return false;
        }
    }
    
    // Check target pattern
    if (!target_pattern.empty()) {
        try {
            std::regex re(target_pattern, std::regex::icase);
            if (!std::regex_search(event.target, re)) return false;
        } catch (...) {
            return false;
        }
    }
    
    return true;
}

inline EventHandlerId EventBus::subscribe(const EventFilter& filter, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    EventHandlerId id = next_handler_id_++;
    subscriptions_.push_back({id, filter, std::move(handler)});
    return id;
}

inline EventHandlerId EventBus::subscribe(EventType type, EventHandler handler) {
    return subscribe(EventFilter::for_type(type), std::move(handler));
}

inline EventHandlerId EventBus::subscribe_all(EventHandler handler) {
    return subscribe(EventFilter::all(), std::move(handler));
}

inline void EventBus::unsubscribe(EventHandlerId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
            [id](const Subscription& s) { return s.id == id; }),
        subscriptions_.end());
}

inline void EventBus::publish(const Event& event) {
    Event e = event;
    e.sequence_number = ++event_counter_;
    
    add_to_history(e);
    dispatch(e);
}

inline void EventBus::publish(EventType type, const std::string& source,
                               const std::string& target, const std::string& message) {
    publish(Event(type, source, target, message));
}

inline void EventBus::dispatch(const Event& event) {
    std::vector<EventHandler> handlers;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& sub : subscriptions_) {
            if (sub.filter.matches(event)) {
                handlers.push_back(sub.handler);
            }
        }
    }
    
    for (const auto& handler : handlers) {
        try {
            handler(event);
        } catch (...) {
            // Ignore handler exceptions
        }
    }
}

inline void EventBus::add_to_history(const Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    history_.push_back(event);
    while (history_.size() > max_history_) {
        history_.erase(history_.begin());
    }
}

inline std::vector<Event> EventBus::get_history(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (count >= history_.size()) {
        return history_;
    }
    
    return std::vector<Event>(history_.end() - static_cast<long>(count), history_.end());
}

inline std::vector<Event> EventBus::get_events_by_type(EventType type, size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Event> result;
    for (auto it = history_.rbegin(); it != history_.rend() && result.size() < count; ++it) {
        if (it->type == type) {
            result.push_back(*it);
        }
    }
    return result;
}

inline std::vector<Event> EventBus::get_events_since(
    const std::chrono::system_clock::time_point& since) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Event> result;
    for (const auto& e : history_) {
        if (e.timestamp >= since) {
            result.push_back(e);
        }
    }
    return result;
}

inline void EventBus::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

inline size_t EventBus::get_subscriber_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.size();
}

// ============================================================================
// Convenience Macros
// ============================================================================

#define TMS_EMIT_EVENT(type, target, message) \
    tms::EventBus::instance().publish(tms::EventType::type, "TMSSystem", target, message)

#define TMS_EMIT_WARNING(type, target, message) \
    tms::EventBus::instance().publish( \
        tms::Event(tms::EventType::type, "TMSSystem", target, message) \
            .with_severity(tms::EventSeverity::WARNING))

#define TMS_EMIT_ERROR(type, target, message) \
    tms::EventBus::instance().publish( \
        tms::Event(tms::EventType::type, "TMSSystem", target, message) \
            .with_severity(tms::EventSeverity::ERROR))

} // namespace tms

#endif // TMS_EVENTS_H
