CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -frounding-math
LIB = src/fpu.cpp ../alu/src/units.cpp

fputest: tests/fputest.cpp $(LIB) include/fpu.hpp
	$(CXX) $(CXXFLAGS) -o $@ tests/fputest.cpp $(LIB)

test: fputest
	./fputest

clean:
	rm -f fputest
