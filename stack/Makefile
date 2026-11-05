CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude
TARGET   := risc201_stack.exe

# Find all .cpp files in the src directory
SRCS     := $(wildcard src/*.cpp)
# Output .o files in a build directory
OBJS     := $(patsubst src/%.cpp, build/%.o, $(SRCS))

.PHONY: all test clean run directories

all: directories $(TARGET)

directories:
	@if not exist "build" mkdir build

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	.\$(TARGET)

test: $(TARGET)
	.\$(TARGET)

clean:
	@if exist "build" rmdir /s /q build
	@if exist "$(TARGET)" del /q $(TARGET)