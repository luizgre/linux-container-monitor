# Linux Container Resource Monitoring System Makefile
# Author: Luiz FG
# Course: Operating Systems - RA3

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE -I./include
LDFLAGS = -lrt
DEBUG_FLAGS = -g -O0
RELEASE_FLAGS = -O2

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

# Source files
SOURCES = $(SRC_DIR)/cpu_monitor.c \
          $(SRC_DIR)/memory_monitor.c \
          $(SRC_DIR)/io_monitor.c \
          $(SRC_DIR)/namespace_analyzer.c \
          $(SRC_DIR)/cgroup_manager.c \
          $(SRC_DIR)/main.c

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Test sources
TEST_SOURCES = $(TEST_DIR)/test_cpu.c \
               $(TEST_DIR)/test_memory.c \
               $(TEST_DIR)/test_io.c

TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

# Target executable
TARGET = $(BIN_DIR)/resource-monitor
TEST_TARGET = $(BIN_DIR)/test-suite

# Header dependencies
HEADERS = $(INC_DIR)/monitor.h \
          $(INC_DIR)/namespace.h \
          $(INC_DIR)/cgroup.h

.PHONY: all clean debug release test valgrind install uninstall help

# Default target
all: release

# Create necessary directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	@echo "Compiling test $<..."
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $< -o $@

# Link main executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "Linking $@..."
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build successful! Binary: $@"

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(BUILD_DIR) $(BIN_DIR)
	@echo "Building in DEBUG mode..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS)" --no-print-directory

# Release build
release: CFLAGS += $(RELEASE_FLAGS)
release: $(TARGET)

# Build tests
test: $(BUILD_DIR) $(BIN_DIR)
	@echo "Building test suite..."
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $(SRC_DIR)/cpu_monitor.c -o $(BUILD_DIR)/cpu_monitor.o
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $(SRC_DIR)/memory_monitor.c -o $(BUILD_DIR)/memory_monitor.o
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -c $(SRC_DIR)/io_monitor.c -o $(BUILD_DIR)/io_monitor.o
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(TEST_DIR)/test_cpu.c $(BUILD_DIR)/cpu_monitor.o -o $(BIN_DIR)/test_cpu $(LDFLAGS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(TEST_DIR)/test_memory.c $(BUILD_DIR)/memory_monitor.o $(BUILD_DIR)/cpu_monitor.o -o $(BIN_DIR)/test_memory $(LDFLAGS)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(TEST_DIR)/test_io.c $(BUILD_DIR)/io_monitor.o $(BUILD_DIR)/cpu_monitor.o -o $(BIN_DIR)/test_io $(LDFLAGS)
	@echo "Test suite built successfully!"
	@echo "Run tests with: make run-tests"

# Run tests
run-tests: test
	@echo "\n=== Running CPU Monitor Tests ==="
	@./$(BIN_DIR)/test_cpu || true
	@echo "\n=== Running Memory Monitor Tests ==="
	@./$(BIN_DIR)/test_memory || true
	@echo "\n=== Running I/O Monitor Tests ==="
	@sudo ./$(BIN_DIR)/test_io || echo "Note: I/O tests require sudo"

# Memory leak check with valgrind
valgrind: debug
	@echo "Running valgrind memory check..."
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         --verbose \
	         --log-file=valgrind-report.txt \
	         $(TARGET) --help
	@echo "Valgrind report saved to valgrind-report.txt"

# Install (copy to /usr/local/bin)
install: release
	@echo "Installing $(TARGET) to /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/resource-monitor
	@sudo chmod +x /usr/local/bin/resource-monitor
	@echo "Installation complete!"

# Uninstall
uninstall:
	@echo "Removing /usr/local/bin/resource-monitor..."
	@sudo rm -f /usr/local/bin/resource-monitor
	@echo "Uninstallation complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -f valgrind-report.txt
	@rm -f *.csv *.json
	@echo "Clean complete!"

# Help target
help:
	@echo "Linux Container Resource Monitoring System - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build release version (default)"
	@echo "  make all          - Same as 'make'"
	@echo "  make release      - Build optimized release version"
	@echo "  make debug        - Build debug version with symbols"
	@echo "  make test         - Build test suite"
	@echo "  make run-tests    - Build and run all tests"
	@echo "  make valgrind     - Run valgrind memory leak check"
	@echo "  make install      - Install to /usr/local/bin (requires sudo)"
	@echo "  make uninstall    - Remove from /usr/local/bin (requires sudo)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Compilation flags:"
	@echo "  CC = $(CC)"
	@echo "  CFLAGS = $(CFLAGS)"
	@echo "  LDFLAGS = $(LDFLAGS)"
	@echo ""
