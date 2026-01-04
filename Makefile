CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS := -lz

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests
BENCHMARK_DIR := benchmarks

APP := main
TEST_APP := tests_runner

# Core source files
SRC_FILES := \
	$(SRC_DIR)/command_parser.cpp \
	$(SRC_DIR)/memtable.cpp \
	$(SRC_DIR)/sstable.cpp \
	$(SRC_DIR)/storage_engine.cpp \
	$(SRC_DIR)/wal.cpp \
	$(SRC_DIR)/test_framework.cpp \
	$(SRC_DIR)/bloom_filter.cpp \
	$(SRC_DIR)/lru_cache.cpp \

SRC_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

# App objects
APP_OBJ := $(BUILD_DIR)/main.o

# Test sources & objects
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/%.test.o,$(TEST_SRCS))

# Benchmark sources & objects
BENCHMARK_SRCS := $(wildcard $(BENCHMARK_DIR)/*.cpp)
BENCHMARK_BINS := $(patsubst $(BENCHMARK_DIR)/%.cpp,$(BUILD_DIR)/%,$(BENCHMARK_SRCS))

# Default target
all: $(BUILD_DIR) $(BUILD_DIR)/$(APP)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.test.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link binaries
$(BUILD_DIR)/$(APP): $(SRC_OBJS) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/$(TEST_APP): $(SRC_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Benchmark targets
$(BUILD_DIR)/benchmark_%: $(BENCHMARK_DIR)/benchmark_%.cpp $(SRC_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

benchmarks: $(BENCHMARK_BINS)

# Commands
run: $(BUILD_DIR)/$(APP)
	./$(BUILD_DIR)/$(APP)

test: $(BUILD_DIR)/$(TEST_APP)
	./$(BUILD_DIR)/$(TEST_APP)

# Run all benchmarks
bench: benchmarks
	@echo "Running all benchmarks..."
	@echo ""
	@./$(BUILD_DIR)/benchmark_write_throughput
	@echo ""
	@./$(BUILD_DIR)/benchmark_read_latency
	@echo ""
	@./$(BUILD_DIR)/benchmark_bloom_filter
	@echo ""
	@./$(BUILD_DIR)/benchmark_compaction

# Run individual benchmarks
bench-write: $(BUILD_DIR)/benchmark_write_throughput
	./$(BUILD_DIR)/benchmark_write_throughput

bench-read: $(BUILD_DIR)/benchmark_read_latency
	./$(BUILD_DIR)/benchmark_read_latency

bench-bloom: $(BUILD_DIR)/benchmark_bloom_filter
	./$(BUILD_DIR)/benchmark_bloom_filter

bench-compaction: $(BUILD_DIR)/benchmark_compaction
	./$(BUILD_DIR)/benchmark_compaction

check:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed."; exit 1; }
	cppcheck --enable=all --inconclusive --std=c++20 \
		--suppress=missingIncludeSystem \
		--suppress=checkersReport \
		--suppress=functionConst \
		--suppress=normalCheckLevelMaxBranches \
		--inline-suppr \
		-Iinclude $(SRC_DIR) $(TEST_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test benchmarks bench bench-write bench-read bench-bloom bench-compaction clean check
