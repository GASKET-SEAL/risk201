CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIB = src/alu.cpp src/units.cpp
HDR = include/alu.hpp include/units.hpp

all: risc201alu difftest

risc201alu: src/main.cpp $(LIB) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(LIB)

difftest: tests/difftest.cpp $(LIB) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ tests/difftest.cpp $(LIB)

test: all
	./risc201alu
	./difftest

clean:
	rm -f risc201alu difftest
