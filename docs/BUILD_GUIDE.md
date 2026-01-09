# TMS Emulation - Build Guide
Version  3.3.0

## Prerequisites

### Required
- C++20 compliant compiler
- CMake 3.16+ (for CMake builds)
- Make (for Makefile builds)

### Supported Compilers
- GCC 10 or later
- Clang 11 or later
- MSVC 2019 (16.8) or later
- MinGW-w64 with GCC 10+

## Building with Make

### Linux/macOS
```bash
# Build library and main executable
make

# Build and run tests
make test

# Build examples
make examples

# Clean build artifacts
make clean

# Build with debug symbols
make DEBUG=1

# Build with specific compiler
make CXX=clang++
```

### Windows (MinGW)
```bash
# Using MinGW Make
mingw32-make

# Build and run tests
mingw32-make test

# Clean
mingw32-make clean
```

## Building with CMake

### Linux/macOS
```bash
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
ctest --output-on-failure

# Install
cmake --install . --prefix /usr/local
```

### Windows (Visual Studio)
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Run tests
ctest -C Release
```

### Windows (MinGW)
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## Build Options

### CMake Options
| Option | Default | Description |
|--------|---------|-------------|
| CMAKE_BUILD_TYPE | Release | Build type (Debug/Release) |
| BUILD_TESTS | ON | Build test suite |
| BUILD_EXAMPLES | ON | Build example applications |

### Make Variables
| Variable | Default | Description |
|----------|---------|-------------|
| CXX | g++ | C++ compiler |
| CXXFLAGS | -std=c++20 -O2 | Compiler flags |
| DEBUG | 0 | Enable debug build |

## Directory Structure After Build

```
TMS_v3.3.0/
+-- build/           # CMake build directory
+-- bin/             # Compiled executables (Make)
+-- lib/             # Compiled libraries (Make)
+-- obj/             # Object files (Make)
```

## Platform-Specific Notes

### Windows
- Use MinGW-w64 for GCC-based builds
- MSVC builds require Windows SDK
- Paths use backslash separator

### Linux
- Ensure libpthread is available
- Filesystem library included in GCC 9+

### macOS
- Xcode Command Line Tools required
- Use Homebrew for GCC if needed

## Troubleshooting

### Common Issues

1. **C++20 not supported**
   - Update compiler to supported version
   - Check compiler flags include -std=c++20

2. **Filesystem not found**
   - GCC 8: Link with -lstdc++fs
   - GCC 9+: Built into standard library

3. **Thread library not found**
   - Add -pthread to compiler flags

4. **MinGW path issues**
   - Use forward slashes in paths
   - Check PATH includes MinGW bin

## Verification

After building, verify installation:
```bash
# Check version
./tms --version

# Run tests
./test_tms

# Run example
./basic_usage
```

## Clean Build

For a fresh build:
```bash
# Make
make clean
make

# CMake
rm -rf build
mkdir build && cd build
cmake ..
cmake --build .
```
