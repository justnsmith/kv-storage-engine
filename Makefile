CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2
TARGET := main
SRC := main.cpp
OBJ := $(patsubst %.cpp,build/%.o,$(SRC))

all: build $(TARGET)

build:
	mkdir -p build

build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o build/$(TARGET) $(OBJ)

run: $(TARGET)
	./build/$(TARGET)

clean:
	rm -rf build

.PHONY: all run clean build
