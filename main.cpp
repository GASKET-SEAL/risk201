#include "../include/assembler.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: risc201asm input.asm [-o outprefix]\n";
        return 1;
    }
    std::string inPath = argv[1];
    std::string outPrefix = "a";
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) outPrefix = argv[++i];
    }

    risc201::Assembler assembler;
    return assembler.assemble(inPath, outPrefix) ? 0 : 1;
}
