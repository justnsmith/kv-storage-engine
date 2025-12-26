CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS := -lz

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests

APP := main
TEST_APP := test_storage_engine

# Core source files (NO main.cpp)
SRC_FILES := \
	$(SRC_DIR)/command_parser.cpp \
	$(SRC_DIR)/memtable.cpp \
	$(SRC_DIR)/sstable.cpp \
	$(SRC_DIR)/storage_engine.cpp \
	$(SRC_DIR)/wal.cpp \
	$(SRC_DIR)/test_framework.cpp

SRC_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

# App + Test objects
MAIN_OBJ := $(BUILD_DIR)/main.o
TEST_OBJ := $(BUILD_DIR)/test_storage_engine.o

# Default target
all: $(BUILD_DIR) $(BUILD_DIR)/$(APP)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_storage_engine.o: $(TEST_DIR)/test_storage_engine.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link binaries
$(BUILD_DIR)/$(APP): $(SRC_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/$(TEST_APP): $(SRC_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Commands
run: $(BUILD_DIR)/$(APP)
	./$(BUILD_DIR)/$(APP)

test: $(BUILD_DIR)/$(TEST_APP)
	./$(BUILD_DIR)/$(TEST_APP)

# Static analysis using cppcheck
check:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed."; exit 1; }
	cppcheck --enable=all --inconclusive --std=c++20 \
		--suppress=missingIncludeSystem \
		--suppress=checkersReport \
		--suppress=functionConst \
		--suppress=normalCheckLevelMaxBranches \
		-Iinclude $(SRC_DIR) $(TEST_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test clean check
