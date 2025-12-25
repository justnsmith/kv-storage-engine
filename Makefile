CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS := -lz

TARGET := main
BUILD_DIR := build
SRC_DIR := src

# Automatically find all .cpp files in src/
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

all: $(BUILD_DIR) $(BUILD_DIR)/$(TARGET)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile each .cpp into build/*.o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link all object files into the final executable
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Run the program
run: all
	./$(BUILD_DIR)/$(TARGET)

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

.PHONY: all run clean check
