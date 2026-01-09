# TMS Emulation
Version  3.3.0

TMS (Tape Management System) Emulation - A modern C++20 implementation of IBM mainframe tape management functionality.

## Overview

This library provides a comprehensive emulation of tape volume management operations found in mainframe environments. It includes volume tracking, dataset management, audit logging, and enterprise features suitable for modern applications.

## Features

### Core Features
- Volume management (add, delete, update, query)
- Dataset tracking and management
- Pool-based organization
- Audit logging with export
- Catalog persistence (JSON format)
- Thread-safe operations

### v3.3.0 Enhancements
- **Encryption Metadata**: Track volume encryption status, algorithm, keys
- **Storage Tiering**: HOT/WARM/COLD/ARCHIVE classification
- **Quota Management**: Pool and owner capacity limits
- **Audit Export**: JSON, CSV, and text format export
- **Config Profiles**: Save and load configuration presets
- **Statistics**: Aggregation with percentile calculations
- **Parallel Batch**: Multi-threaded bulk operations
- **Error Recovery**: Automatic retry with exponential backoff

### v3.2.0 Features
- Volume snapshots with point-in-time state capture
- Health scoring with multi-factor assessment
- Fuzzy search using Levenshtein distance
- Lifecycle recommendations
- Location history tracking
- Pool migration operations

### v3.0.0 Features
- JSON serialization
- Volume groups
- Retention policies
- Report generation
- Backup management
- Event system
- Query engine

## Quick Start

```cpp
#include "tms_tape_mgmt.h"
using namespace tms;

int main() {
    // Initialize system
    TMSSystem tms("./catalog");
    
    // Add a volume
    TapeVolume vol;
    vol.volser = "VOL001";
    vol.status = VolumeStatus::SCRATCH;
    vol.pool = "PROD";
    tms.add_volume(vol);
    
    // v3.3.0: Set encryption
    EncryptionMetadata enc;
    enc.encrypted = true;
    enc.algorithm = EncryptionAlgorithm::AES_256;
    enc.key_id = "KEY001";
    tms.set_volume_encryption("VOL001", enc);
    
    // v3.3.0: Set storage tier
    tms.set_volume_tier("VOL001", StorageTier::HOT);
    
    // v3.3.0: Set quota
    Quota quota;
    quota.name = "PROD";
    quota.max_bytes = 1000000000;
    quota.max_volumes = 100;
    tms.set_pool_quota("PROD", quota);
    
    // v3.3.0: Export audit log
    std::string csv = tms.export_audit_log(AuditExportFormat::CSV);
    
    return 0;
}
```

## Building

### Using CMake
```bash
mkdir build && cd build
cmake ..
make
```

### Using Make
```bash
make
make test
```

## Directory Structure

```
TMS_v3.3.0/
|--- include/        # Header files
|--- src/            # Source files
|--- tests/          # Test suite (320 tests)
|--- docs/           # Documentation
|--- examples/       # Usage examples
|--- config/         # Configuration files
|--- CMakeLists.txt  # CMake build
\--- Makefile        # Make build
```

## Requirements

- C++20 compatible compiler
- pthread support
- No external dependencies

## Platform Support

- Windows (MinGW, MSVC)
- Linux (GCC, Clang)
- macOS (Apple Clang)

## Test Results

```
Total:  320 tests
Passed: 320
Failed: 0
```

## Documentation

- [API Reference](API_REFERENCE.md)
- [Build Guide](BUILD_GUIDE.md)
- [Change Log](CHANGELOG.md)
- [Background](BACKGROUND.md)

## License

MIT License - See [LICENSE.txt](LICENSE.txt)

## Author

Bennie Shearer

## Copyright

Copyright (c) 2025 Bennie Shearer
