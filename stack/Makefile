CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:src/%.cpp=build/%.o)
BIN      := risc201_stack

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp $(wildcard include/*.hpp) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

test: $(BIN)
	./$(BIN) && ./$(BIN) --naive && ./$(BIN) --no-canary && ./$(BIN) --no-shadow \
	  && ./$(BIN) --no-canary --no-shadow | tail -1

clean:
	rm -rf build $(BIN)

.PHONY: all test clean
