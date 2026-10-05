#include "../include/disassembler.hpp"
#include "../include/reader.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: risc201disasm input.hex|input.bin [-o outfile] [-s symfile]\n";
        return 1;
    }
    std::string inPath = argv[1];
    std::string outPath;
    std::string symPath;
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) outPath = argv[++i];
        else if (arg == "-s" && i + 1 < argc) symPath = argv[++i];
    }

    risc201::Disassembler disasm;
    std::string listing;
    try {
        std::vector<uint32_t> words = risc201::Reader::readWords(inPath);
        std::map<uint32_t, std::string> symbols;
        if (!symPath.empty()) symbols = risc201::Reader::readSymbols(symPath);
        listing = disasm.disassembleWords(words, symbols);
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