# TMS Tape Management System Emulation
# Makefile Build Configuration
# Version 3.3.0

# Compiler settings
CXX ?= g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -I./include
LDFLAGS =

# Debug/Release
ifeq ($(DEBUG),1)
    CXXFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O2 -DNDEBUG
endif

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -pthread
endif
ifeq ($(UNAME_S),Darwin)
    # macOS settings
endif

# Windows/MinGW
ifeq ($(OS),Windows_NT)
    EXE_EXT = .exe
    RM = del /Q
    MKDIR = mkdir
else
    EXE_EXT =
    RM = rm -f
    MKDIR = mkdir -p
endif

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
EXAMPLE_DIR = examples
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SRCS = $(SRC_DIR)/tms_tape_mgmt.cpp \
       $(SRC_DIR)/logger.cpp \
       $(SRC_DIR)/configuration.cpp

OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Targets
MAIN_TARGET = $(BIN_DIR)/tms$(EXE_EXT)
TEST_TARGET = $(BIN_DIR)/test_tms$(EXE_EXT)
EXAMPLE_TARGET = $(BIN_DIR)/basic_usage$(EXE_EXT)

# Default target
all: dirs $(MAIN_TARGET)

# Create directories
dirs:
	@$(MKDIR) $(OBJ_DIR) 2>/dev/null || true
	@$(MKDIR) $(BIN_DIR) 2>/dev/null || true

# Main executable
$(MAIN_TARGET): $(OBJS) $(OBJ_DIR)/tms_main.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Test executable
test: dirs $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(OBJS) $(OBJ_DIR)/test_tms.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Example executable
examples: dirs $(EXAMPLE_TARGET)

$(EXAMPLE_TARGET): $(OBJS) $(OBJ_DIR)/basic_usage.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/test_tms.o: $(TEST_DIR)/test_tms.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJ_DIR)/basic_usage.o: $(EXAMPLE_DIR)/basic_usage.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean
clean:
	$(RM) $(OBJ_DIR)/*.o 2>/dev/null || true
	$(RM) $(MAIN_TARGET) $(TEST_TARGET) $(EXAMPLE_TARGET) 2>/dev/null || true

# Rebuild
rebuild: clean all

# Install (Linux/macOS)
install: all
	install -d /usr/local/bin
	install -m 755 $(MAIN_TARGET) /usr/local/bin/
	install -d /usr/local/include/tms
	install -m 644 $(INC_DIR)/*.h /usr/local/include/tms/

# Uninstall
uninstall:
	$(RM) /usr/local/bin/tms$(EXE_EXT)
	$(RM) -r /usr/local/include/tms

# Help
help:
	@echo "TMS Build System v3.3.0"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build main executable (default)"
	@echo "  test      - Build and run tests"
	@echo "  examples  - Build example application"
	@echo "  clean     - Remove build artifacts"
	@echo "  rebuild   - Clean and build"
	@echo "  install   - Install to /usr/local"
	@echo "  uninstall - Remove installation"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG=1   - Build with debug symbols"
	@echo "  CXX=...   - Specify compiler"

.PHONY: all dirs test examples clean rebuild install uninstall help
