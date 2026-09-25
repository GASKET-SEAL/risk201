#include "../include/disassembler.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: risc201disasm input.hex|input.bin [-o outfile]\n";
        return 1;
    }
    std::string inPath = argv[1];
    std::string outPath;
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) outPath = argv[++i];
    }

    risc201::Disassembler disasm;
    std::string listing;
    try {
        listing = disasm.disassemble(inPath);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }

    std::cout << listing;
    if (!outPath.empty()) {
        std::ofstream out(outPath);
        out << listing;
    }
    return 0;
}
