CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS := -lz

TARGET := main
TEST_TARGET := test_storage_engine

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Exclude main.o for tests
LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

TEST_SRC := $(TEST_DIR)/test_storage_engine.cpp
TEST_OBJ := $(BUILD_DIR)/test_storage_engine.o

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile src/*.cpp → build/*.o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile tests/*.cpp → build/*.o
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link main binary
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Link test binary (no main.o)
$(BUILD_DIR)/$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Run app
run: all
	./$(BUILD_DIR)/$(TARGET)

# Run tests
test: $(BUILD_DIR) $(BUILD_DIR)/$(TEST_TARGET)
	./$(BUILD_DIR)/$(TEST_TARGET)

# Static analysis
check:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed."; exit 1; }
	cppcheck --enable=all --inconclusive --std=c++20 \
		--suppress=missingIncludeSystem \
		--suppress=checkersReport \
		--suppress=functionConst \
		-Iinclude $(SRC_DIR)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run test clean check
