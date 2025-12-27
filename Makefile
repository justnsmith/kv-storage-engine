CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS := -lz

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests

APP := main
TEST_APP := tests_runner

# Core source files
SRC_FILES := \
	$(SRC_DIR)/command_parser.cpp \
	$(SRC_DIR)/memtable.cpp \
	$(SRC_DIR)/sstable.cpp \
	$(SRC_DIR)/storage_engine.cpp \
	$(SRC_DIR)/wal.cpp \
	$(SRC_DIR)/test_framework.cpp

SRC_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))

# App objects
APP_OBJ := $(BUILD_DIR)/main.o

# Test sources & objects
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/%.test.o,$(TEST_SRCS))

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

# Commands
run: $(BUILD_DIR)/$(APP)
	./$(BUILD_DIR)/$(APP)

test: $(BUILD_DIR)/$(TEST_APP)
	./$(BUILD_DIR)/$(TEST_APP)

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
