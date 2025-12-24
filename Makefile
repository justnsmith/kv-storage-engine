CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2
LDFLAGS := -lz
TARGET := main
SRC := main.cpp
OBJ := $(patsubst %.cpp,build/%.o,$(SRC))

all: build $(TARGET)

build:
	mkdir -p build

build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o build/$(TARGET) $(OBJ) $(LDFLAGS)

run: $(TARGET)
	./build/$(TARGET)

check:
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not installed. Please install it."; exit 1; }
	cppcheck --enable=all --inconclusive --std=c++20 \
		--suppress=missingIncludeSystem \
		--suppress=checkersReport \
		$(SRC)

clean:
	rm -rf build

.PHONY: all run clean build check
