/**
 * @file tms_tape_mgmt.h
 * @brief TMS Tape Management System - Core Library (Compatibility Header)
 * @version 3.3.0
 * @date 2026-01-08
 * @author Bennie Shearer
 * @copyright Copyright (c) 2025 Bennie Shearer
 * @license MIT License
 *
 * This header provides backward compatibility by including all component headers.
 * For new code, prefer including specific headers:
 *   - tms_types.h      - Data types and enumerations
 *   - tms_utils.h      - Utility functions
 *   - tms_system.h     - TMSSystem class
 *   - tms_json.h       - JSON serialization (v3.0.0)
 *   - tms_groups.h     - Volume groups (v3.0.0)
 *   - tms_retention.h  - Retention policies (v3.0.0)
 *   - tms_reports.h    - Report generation (v3.0.0)
 *   - tms_backup.h     - Backup rotation (v3.0.0)
 *   - tms_events.h     - Event system (v3.0.0)
 *   - tms_history.h    - Statistics history (v3.0.0)
 *   - tms_integrity.h  - Integrity verification (v3.0.0)
 *   - tms_query.h      - Query language (v3.0.0)
 */

#ifndef TMS_TAPE_MGMT_H
#define TMS_TAPE_MGMT_H

// Core headers
#include "tms_version.h"
#include "error_codes.h"
#include "logger.h"
#include "configuration.h"
#include "tms_types.h"
#include "tms_utils.h"
#include "tms_system.h"

// v3.0.0 Enhancement headers
#include "tms_json.h"
#include "tms_groups.h"
#include "tms_retention.h"
#include "tms_reports.h"
#include "tms_backup.h"
#include "tms_events.h"
#include "tms_history.h"
#include "tms_integrity.h"
#include "tms_query.h"

#endif // TMS_TAPE_MGMT_H
