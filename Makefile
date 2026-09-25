CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

risc201asm: src/*.cpp
	$(CXX) $(CXXFLAGS) -o risc201asm src/*.cpp

clean:
	rm -f risc201asm *.hex *.bin
