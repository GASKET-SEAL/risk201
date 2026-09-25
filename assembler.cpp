#include "../include/assembler.hpp"
#include "../include/assembler_pass1.hpp"
#include "../include/assembler_pass2.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>

namespace risc201 {

bool Assembler::assemble(const std::string& inputPath, const std::string& outputPrefix) {
    std::vector<SourceLine> source;
    try {
        source = parser.parseFile(inputPath);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return false;
    }

    AssemblerPass1 pass1(opcodeTable, symbols, diagnostics);
    std::vector<IntermediateRecord> intermediate = pass1.run(source);

    AssemblerPass2 pass2(opcodeTable, symbols, diagnostics);
    std::vector<EncodedRecord> encoded = pass2.run(intermediate);

    if (diagnostics.hasErrors()) {
        for (const auto& d : diagnostics.all())
            std::cerr << inputPath << ":" << d.lineNumber << ": error: " << d.message << "\n";
        std::cerr << diagnostics.all().size() << " error(s), no output written.\n";
        return false;
    }

    std::ofstream hex(outputPrefix + ".hex");
    for (const auto& e : encoded) {
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", e.machineWord);
        hex << buf << "\n";
    }

    std::ofstream bin(outputPrefix + ".bin", std::ios::binary);
    for (const auto& e : encoded) {
        unsigned char bytes[4] = {
            (unsigned char)((e.machineWord >> 24) & 0xFF),
            (unsigned char)((e.machineWord >> 16) & 0xFF),
            (unsigned char)((e.machineWord >> 8) & 0xFF),
            (unsigned char)(e.machineWord & 0xFF),
        };
        bin.write((char*)bytes, 4);
    }

    std::cout << "assembled " << encoded.size() << " instruction(s) -> "
              << outputPrefix << ".hex, " << outputPrefix << ".bin\n";
    return true;
}

} // namespace risc201
