# TMS Tape Management System Emulation
# CMake Build Configuration
# Version 3.3.0

cmake_minimum_required(VERSION 3.16)

project(TMS_Emulation     
    VERSION 3.3.0     
    DESCRIPTION "TMS (Tape Management System) Emulation"     
    LANGUAGES CXX 
)

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific settings
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(TMS_SOURCES
    src/tms_tape_mgmt.cpp
    src/logger.cpp
    src/configuration.cpp
)

# Library
add_library(tms_lib STATIC ${TMS_SOURCES})

# Main executable
add_executable(tms src/tms_main.cpp)
target_link_libraries(tms tms_lib)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(tms pthread)
endif()

# Test executable
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    add_executable(test_tms tests/test_tms.cpp)
    target_link_libraries(test_tms tms_lib)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_tms pthread)
    endif()
    enable_testing()
    add_test(NAME TMS_Tests COMMAND test_tms)
endif()

# Example executable
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_executable(basic_usage examples/basic_usage.cpp)
    target_link_libraries(basic_usage tms_lib)
    if(UNIX AND NOT APPLE)
        target_link_libraries(basic_usage pthread)
    endif()
endif()

# Installation
install(TARGETS tms DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/tms)
install(FILES config/tms.conf DESTINATION etc/tms)
