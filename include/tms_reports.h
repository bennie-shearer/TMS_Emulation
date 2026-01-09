/**
 * @file tms_reports.h
 * @brief TMS Tape Management System - Report Generation
 * @version 3.3.0
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * Provides multi-format report generation (Text, HTML, Markdown)
 * for volumes, datasets, pools, and system statistics.
 */

#ifndef TMS_REPORTS_H
#define TMS_REPORTS_H

#include "tms_types.h"
#include "tms_utils.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <functional>

namespace tms {

// ============================================================================
// Report Types
// ============================================================================

/**
 * @brief Report output format
 */
enum class ReportFormat {
    TEXT,           ///< Plain text
    HTML,           ///< HTML with CSS styling
    MARKDOWN,       ///< Markdown format
    CSV             ///< Comma-separated values
};

/**
 * @brief Report content type
 */
enum class ReportType {
    VOLUME_SUMMARY,
    VOLUME_DETAIL,
    DATASET_SUMMARY,
    DATASET_DETAIL,
    POOL_SUMMARY,
    POOL_DETAIL,
    SYSTEM_STATISTICS,
    EXPIRATION_REPORT,
    AUDIT_REPORT,
    HEALTH_REPORT,
    CAPACITY_REPORT,
    INVENTORY_REPORT
};

/**
 * @brief Report options
 */
struct ReportOptions {
    std::string title;
    std::string subtitle;
    bool include_header = true;
    bool include_footer = true;
    bool include_timestamp = true;
    bool include_summary = true;
    bool include_charts = false;  // HTML only
    std::string css_class = "tms-report";
    int max_rows = 0;  // 0 = unlimited
    std::string filter_owner;
    std::string filter_pool;
    std::optional<VolumeStatus> filter_status;
};

// ============================================================================
// Report Generator
// ============================================================================

/**
 * @brief Multi-format report generator
 */
class ReportGenerator {
public:
    using VolumeListCallback = std::function<std::vector<TapeVolume>()>;
    using DatasetListCallback = std::function<std::vector<Dataset>()>;
    using StatisticsCallback = std::function<SystemStatistics()>;
    using AuditCallback = std::function<std::vector<AuditRecord>(size_t)>;
    using HealthCallback = std::function<HealthCheckResult()>;
    
    ReportGenerator() = default;
    
    // Volume reports
    std::string generate_volume_report(const std::vector<TapeVolume>& volumes,
                                        ReportFormat format,
                                        const ReportOptions& options = ReportOptions{});
    
    std::string generate_volume_detail(const TapeVolume& volume,
                                        ReportFormat format);
    
    // Dataset reports
    std::string generate_dataset_report(const std::vector<Dataset>& datasets,
                                         ReportFormat format,
                                         const ReportOptions& options = ReportOptions{});
    
    std::string generate_dataset_detail(const Dataset& dataset,
                                          ReportFormat format);
    
    // Pool reports
    std::string generate_pool_report(const std::vector<PoolStatistics>& pools,
                                      ReportFormat format,
                                      const ReportOptions& options = ReportOptions{});
    
    // Statistics reports
    std::string generate_statistics_report(const SystemStatistics& stats,
                                            ReportFormat format,
                                            const ReportOptions& options = ReportOptions{});
    
    // Expiration reports
    std::string generate_expiration_report(const std::vector<TapeVolume>& volumes,
                                            const std::vector<Dataset>& datasets,
                                            ReportFormat format,
                                            std::chrono::hours lookahead = std::chrono::hours(168));
    
    // Audit reports
    std::string generate_audit_report(const std::vector<AuditRecord>& records,
                                       ReportFormat format,
                                       const ReportOptions& options = ReportOptions{});
    
    // Health reports
    std::string generate_health_report(const HealthCheckResult& health,
                                        ReportFormat format);
    
    // Capacity reports
    std::string generate_capacity_report(const std::vector<TapeVolume>& volumes,
                                          ReportFormat format);
    
    // File output
    bool write_to_file(const std::string& content, const std::string& path);
    
private:
    // Format helpers
    std::string text_header(const std::string& title, char underline = '=');
    std::string text_table_row(const std::vector<std::string>& cols,
                                const std::vector<int>& widths);
    std::string text_separator(const std::vector<int>& widths, char sep = '-');
    
    std::string html_header(const std::string& title, const std::string& css_class);
    std::string html_footer();
    std::string html_table_start(const std::vector<std::string>& headers);
    std::string html_table_row(const std::vector<std::string>& cols);
    std::string html_table_end();
    std::string html_escape(const std::string& str);
    
    std::string md_header(const std::string& title, int level = 1);
    std::string md_table_header(const std::vector<std::string>& headers);
    std::string md_table_row(const std::vector<std::string>& cols);
    
    std::string csv_row(const std::vector<std::string>& cols);
    std::string csv_escape(const std::string& str);
    
    std::string get_css_styles();
};

// ============================================================================
// Implementation
// ============================================================================

inline std::string ReportGenerator::generate_volume_report(
    const std::vector<TapeVolume>& volumes,
    ReportFormat format,
    const ReportOptions& options) {
    
    std::ostringstream oss;
    std::string title = options.title.empty() ? "Volume Report" : options.title;
    
    switch (format) {
        case ReportFormat::TEXT: {
            oss << text_header(title);
            if (options.include_timestamp) {
                oss << "Generated: " << get_timestamp() << "\n\n";
            }
            
            std::vector<int> widths = {8, 10, 10, 12, 10, 12};
            oss << text_table_row({"Volser", "Status", "Density", "Pool", "Owner", "Used"}, widths);
            oss << text_separator(widths);
            
            int count = 0;
            for (const auto& vol : volumes) {
                if (options.max_rows > 0 && count >= options.max_rows) break;
                if (!options.filter_owner.empty() && vol.owner != options.filter_owner) continue;
                if (!options.filter_pool.empty() && vol.pool != options.filter_pool) continue;
                if (options.filter_status && vol.status != *options.filter_status) continue;
                
                oss << text_table_row({
                    vol.volser,
                    volume_status_to_string(vol.status),
                    density_to_string(vol.density),
                    vol.pool,
                    vol.owner,
                    format_bytes(vol.used_bytes)
                }, widths);
                count++;
            }
            
            if (options.include_summary) {
                oss << "\nTotal: " << count << " volumes\n";
            }
            break;
        }
        
        case ReportFormat::HTML: {
            oss << html_header(title, options.css_class);
            if (options.include_timestamp) {
                oss << "<p class=\"timestamp\">Generated: " << get_timestamp() << "</p>\n";
            }
            
            oss << html_table_start({"Volser", "Status", "Density", "Pool", "Owner", "Used", "Datasets"});
            
            int count = 0;
            for (const auto& vol : volumes) {
                if (options.max_rows > 0 && count >= options.max_rows) break;
                if (!options.filter_owner.empty() && vol.owner != options.filter_owner) continue;
                
                oss << html_table_row({
                    vol.volser,
                    volume_status_to_string(vol.status),
                    density_to_string(vol.density),
                    vol.pool,
                    vol.owner,
                    format_bytes(vol.used_bytes),
                    std::to_string(vol.datasets.size())
                });
                count++;
            }
            
            oss << html_table_end();
            if (options.include_summary) {
                oss << "<p class=\"summary\">Total: " << count << " volumes</p>\n";
            }
            oss << html_footer();
            break;
        }
        
        case ReportFormat::MARKDOWN: {
            oss << md_header(title);
            if (options.include_timestamp) {
                oss << "*Generated: " << get_timestamp() << "*\n\n";
            }
            
            oss << md_table_header({"Volser", "Status", "Density", "Pool", "Owner", "Used"});
            
            int count = 0;
            for (const auto& vol : volumes) {
                if (options.max_rows > 0 && count >= options.max_rows) break;
                
                oss << md_table_row({
                    vol.volser,
                    volume_status_to_string(vol.status),
                    density_to_string(vol.density),
                    vol.pool,
                    vol.owner,
                    format_bytes(vol.used_bytes)
                });
                count++;
            }
            
            if (options.include_summary) {
                oss << "\n**Total:** " << count << " volumes\n";
            }
            break;
        }
        
        case ReportFormat::CSV: {
            oss << csv_row({"Volser", "Status", "Density", "Location", "Pool", "Owner",
                            "MountCount", "Capacity", "Used", "Created", "Expires"});
            
            for (const auto& vol : volumes) {
                oss << csv_row({
                    vol.volser,
                    volume_status_to_string(vol.status),
                    density_to_string(vol.density),
                    vol.location,
                    vol.pool,
                    vol.owner,
                    std::to_string(vol.mount_count),
                    std::to_string(vol.capacity_bytes),
                    std::to_string(vol.used_bytes),
                    format_time(vol.creation_date),
                    format_time(vol.expiration_date)
                });
            }
            break;
        }
    }
    
    return oss.str();
}

inline std::string ReportGenerator::generate_statistics_report(
    const SystemStatistics& stats,
    ReportFormat format,
    const ReportOptions& options) {
    
    std::ostringstream oss;
    std::string title = options.title.empty() ? "System Statistics" : options.title;
    
    switch (format) {
        case ReportFormat::TEXT: {
            oss << text_header(title);
            oss << "Generated: " << get_timestamp() << "\n";
            oss << "Uptime: " << stats.get_uptime() << "\n\n";
            
            oss << "VOLUMES\n";
            oss << "  Total:     " << std::setw(10) << stats.total_volumes << "\n";
            oss << "  Scratch:   " << std::setw(10) << stats.scratch_volumes << "\n";
            oss << "  Private:   " << std::setw(10) << stats.private_volumes << "\n";
            oss << "  Mounted:   " << std::setw(10) << stats.mounted_volumes << "\n";
            oss << "  Expired:   " << std::setw(10) << stats.expired_volumes << "\n";
            oss << "  Reserved:  " << std::setw(10) << stats.reserved_volumes << "\n\n";
            
            oss << "DATASETS\n";
            oss << "  Total:     " << std::setw(10) << stats.total_datasets << "\n";
            oss << "  Active:    " << std::setw(10) << stats.active_datasets << "\n";
            oss << "  Migrated:  " << std::setw(10) << stats.migrated_datasets << "\n";
            oss << "  Expired:   " << std::setw(10) << stats.expired_datasets << "\n\n";
            
            oss << "CAPACITY\n";
            oss << "  Total:       " << format_bytes(stats.total_capacity) << "\n";
            oss << "  Used:        " << format_bytes(stats.used_capacity) << "\n";
            oss << "  Utilization: " << std::fixed << std::setprecision(1) 
                << stats.get_utilization() << "%\n";
            break;
        }
        
        case ReportFormat::HTML: {
            oss << html_header(title, options.css_class);
            oss << "<div class=\"stats-grid\">\n";
            
            oss << "<div class=\"stat-box\"><h3>Volumes</h3>\n";
            oss << "<table>\n";
            oss << "<tr><td>Total</td><td>" << stats.total_volumes << "</td></tr>\n";
            oss << "<tr><td>Scratch</td><td>" << stats.scratch_volumes << "</td></tr>\n";
            oss << "<tr><td>Private</td><td>" << stats.private_volumes << "</td></tr>\n";
            oss << "<tr><td>Mounted</td><td>" << stats.mounted_volumes << "</td></tr>\n";
            oss << "</table></div>\n";
            
            oss << "<div class=\"stat-box\"><h3>Datasets</h3>\n";
            oss << "<table>\n";
            oss << "<tr><td>Total</td><td>" << stats.total_datasets << "</td></tr>\n";
            oss << "<tr><td>Active</td><td>" << stats.active_datasets << "</td></tr>\n";
            oss << "<tr><td>Migrated</td><td>" << stats.migrated_datasets << "</td></tr>\n";
            oss << "</table></div>\n";
            
            oss << "<div class=\"stat-box\"><h3>Capacity</h3>\n";
            oss << "<table>\n";
            oss << "<tr><td>Total</td><td>" << format_bytes(stats.total_capacity) << "</td></tr>\n";
            oss << "<tr><td>Used</td><td>" << format_bytes(stats.used_capacity) << "</td></tr>\n";
            oss << "<tr><td>Utilization</td><td>" << std::fixed << std::setprecision(1) 
                << stats.get_utilization() << "%</td></tr>\n";
            oss << "</table></div>\n";
            
            oss << "</div>\n";
            oss << html_footer();
            break;
        }
        
        case ReportFormat::MARKDOWN: {
            oss << md_header(title);
            oss << "*Generated: " << get_timestamp() << "*\n\n";
            oss << "*Uptime: " << stats.get_uptime() << "*\n\n";
            
            oss << md_header("Volumes", 2);
            oss << "| Metric | Value |\n|--------|-------|\n";
            oss << "| Total | " << stats.total_volumes << " |\n";
            oss << "| Scratch | " << stats.scratch_volumes << " |\n";
            oss << "| Private | " << stats.private_volumes << " |\n";
            oss << "| Mounted | " << stats.mounted_volumes << " |\n\n";
            
            oss << md_header("Capacity", 2);
            oss << "| Metric | Value |\n|--------|-------|\n";
            oss << "| Total | " << format_bytes(stats.total_capacity) << " |\n";
            oss << "| Used | " << format_bytes(stats.used_capacity) << " |\n";
            oss << "| Utilization | " << std::fixed << std::setprecision(1) 
                << stats.get_utilization() << "% |\n";
            break;
        }
        
        default:
            break;
    }
    
    return oss.str();
}

inline std::string ReportGenerator::generate_health_report(
    const HealthCheckResult& health,
    ReportFormat format) {
    
    std::ostringstream oss;
    
    switch (format) {
        case ReportFormat::TEXT: {
            oss << text_header("Health Check Report");
            oss << "Generated: " << get_timestamp() << "\n";
            oss << "Status: " << (health.healthy ? "HEALTHY" : "UNHEALTHY") << "\n\n";
            
            if (!health.errors.empty()) {
                oss << "ERRORS:\n";
                for (const auto& err : health.errors) {
                    oss << "  [X] " << err << "\n";
                }
                oss << "\n";
            }
            
            if (!health.warnings.empty()) {
                oss << "WARNINGS:\n";
                for (const auto& warn : health.warnings) {
                    oss << "  [!] " << warn << "\n";
                }
                oss << "\n";
            }
            
            if (!health.metrics.empty()) {
                oss << "METRICS:\n";
                for (const auto& [key, value] : health.metrics) {
                    oss << "  " << key << ": " << value << "\n";
                }
            }
            break;
        }
        
        case ReportFormat::HTML: {
            oss << html_header("Health Check Report", "health-report");
            oss << "<div class=\"status " << (health.healthy ? "healthy" : "unhealthy") << "\">\n";
            oss << "<h2>Status: " << (health.healthy ? "HEALTHY" : "UNHEALTHY") << "</h2>\n";
            oss << "</div>\n";
            
            if (!health.errors.empty()) {
                oss << "<div class=\"errors\"><h3>Errors</h3><ul>\n";
                for (const auto& err : health.errors) {
                    oss << "<li>" << html_escape(err) << "</li>\n";
                }
                oss << "</ul></div>\n";
            }
            
            if (!health.warnings.empty()) {
                oss << "<div class=\"warnings\"><h3>Warnings</h3><ul>\n";
                for (const auto& warn : health.warnings) {
                    oss << "<li>" << html_escape(warn) << "</li>\n";
                }
                oss << "</ul></div>\n";
            }
            
            oss << html_footer();
            break;
        }
        
        case ReportFormat::MARKDOWN: {
            oss << md_header("Health Check Report");
            oss << "**Status:** " << (health.healthy ? "HEALTHY" : "UNHEALTHY") << "\n\n";
            
            if (!health.errors.empty()) {
                oss << md_header("Errors", 2);
                for (const auto& err : health.errors) {
                    oss << "- :x: " << err << "\n";
                }
                oss << "\n";
            }
            
            if (!health.warnings.empty()) {
                oss << md_header("Warnings", 2);
                for (const auto& warn : health.warnings) {
                    oss << "- :warning: " << warn << "\n";
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    return oss.str();
}

// Helper implementations
inline std::string ReportGenerator::text_header(const std::string& title, char underline) {
    std::ostringstream oss;
    oss << "\n" << title << "\n";
    oss << std::string(title.length(), underline) << "\n";
    return oss.str();
}

inline std::string ReportGenerator::text_table_row(const std::vector<std::string>& cols,
                                                    const std::vector<int>& widths) {
    std::ostringstream oss;
    for (size_t i = 0; i < cols.size() && i < widths.size(); i++) {
        oss << std::left << std::setw(widths[i]) << cols[i];
    }
    oss << "\n";
    return oss.str();
}

inline std::string ReportGenerator::text_separator(const std::vector<int>& widths, char sep) {
    std::ostringstream oss;
    for (int w : widths) {
        oss << std::string(w, sep);
    }
    oss << "\n";
    return oss.str();
}

inline std::string ReportGenerator::html_header(const std::string& title, const std::string& css_class) {
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n<html>\n<head>\n";
    oss << "<meta charset=\"UTF-8\">\n";
    oss << "<title>" << html_escape(title) << "</title>\n";
    oss << "<style>\n" << get_css_styles() << "</style>\n";
    oss << "</head>\n<body class=\"" << css_class << "\">\n";
    oss << "<h1>" << html_escape(title) << "</h1>\n";
    return oss.str();
}

inline std::string ReportGenerator::html_footer() {
    return "</body>\n</html>\n";
}

inline std::string ReportGenerator::html_table_start(const std::vector<std::string>& headers) {
    std::ostringstream oss;
    oss << "<table class=\"data-table\">\n<thead><tr>\n";
    for (const auto& h : headers) {
        oss << "<th>" << html_escape(h) << "</th>\n";
    }
    oss << "</tr></thead>\n<tbody>\n";
    return oss.str();
}

inline std::string ReportGenerator::html_table_row(const std::vector<std::string>& cols) {
    std::ostringstream oss;
    oss << "<tr>";
    for (const auto& c : cols) {
        oss << "<td>" << html_escape(c) << "</td>";
    }
    oss << "</tr>\n";
    return oss.str();
}

inline std::string ReportGenerator::html_table_end() {
    return "</tbody>\n</table>\n";
}

inline std::string ReportGenerator::html_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '&': oss << "&amp;"; break;
            case '<': oss << "&lt;"; break;
            case '>': oss << "&gt;"; break;
            case '"': oss << "&quot;"; break;
            default: oss << c;
        }
    }
    return oss.str();
}

inline std::string ReportGenerator::md_header(const std::string& title, int level) {
    return std::string(level, '#') + " " + title + "\n\n";
}

inline std::string ReportGenerator::md_table_header(const std::vector<std::string>& headers) {
    std::ostringstream oss;
    oss << "|";
    for (const auto& h : headers) oss << " " << h << " |";
    oss << "\n|";
    for (size_t i = 0; i < headers.size(); i++) oss << "------|";
    oss << "\n";
    return oss.str();
}

inline std::string ReportGenerator::md_table_row(const std::vector<std::string>& cols) {
    std::ostringstream oss;
    oss << "|";
    for (const auto& c : cols) oss << " " << c << " |";
    oss << "\n";
    return oss.str();
}

inline std::string ReportGenerator::csv_row(const std::vector<std::string>& cols) {
    std::ostringstream oss;
    for (size_t i = 0; i < cols.size(); i++) {
        if (i > 0) oss << ",";
        oss << csv_escape(cols[i]);
    }
    oss << "\n";
    return oss.str();
}

inline std::string ReportGenerator::csv_escape(const std::string& str) {
    if (str.find_first_of(",\"\n") == std::string::npos) {
        return str;
    }
    std::ostringstream oss;
    oss << '"';
    for (char c : str) {
        if (c == '"') oss << "\"\"";
        else oss << c;
    }
    oss << '"';
    return oss.str();
}

inline bool ReportGenerator::write_to_file(const std::string& content, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

inline std::string ReportGenerator::get_css_styles() {
    return R"(
body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #f5f5f5; }
h1 { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }
h2, h3 { color: #555; }
.timestamp { color: #888; font-style: italic; }
.summary { font-weight: bold; margin-top: 20px; }
.data-table { border-collapse: collapse; width: 100%; margin: 20px 0; background: white; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.data-table th { background: #007acc; color: white; padding: 12px 15px; text-align: left; }
.data-table td { padding: 10px 15px; border-bottom: 1px solid #ddd; }
.data-table tr:hover { background: #f8f8f8; }
.stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }
.stat-box { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
.stat-box h3 { margin-top: 0; color: #007acc; }
.stat-box table { width: 100%; }
.stat-box td { padding: 5px 0; }
.status { padding: 20px; border-radius: 8px; text-align: center; }
.status.healthy { background: #d4edda; color: #155724; }
.status.unhealthy { background: #f8d7da; color: #721c24; }
.errors { background: #fff3cd; padding: 15px; border-radius: 8px; margin: 10px 0; }
.warnings { background: #fff3cd; padding: 15px; border-radius: 8px; margin: 10px 0; }
)";
}

} // namespace tms

#endif // TMS_REPORTS_H
