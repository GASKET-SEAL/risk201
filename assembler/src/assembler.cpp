#include "../include/assembler.hpp"
#include "../include/assembler_pass1.hpp"
#include "../include/assembler_pass2.hpp"
#include "../include/symbol_table.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace risc201 {

AssembleResult Assembler::assembleText(const std::string& sourceText) {
    AssembleResult result;
    SymbolTable symbols;
    Diagnostics diagnostics;

    std::vector<SourceLine> source = parser.parseText(sourceText);

    AssemblerPass1 pass1(opcodeTable, symbols, diagnostics);
    std::vector<IntermediateRecord> intermediate = pass1.run(source);

    AssemblerPass2 pass2(opcodeTable, symbols, diagnostics);
    std::vector<EncodedRecord> encoded = pass2.run(intermediate);

    result.success = !diagnostics.hasErrors();
    result.words = encoded;
    result.symbols = symbols.all();
    result.diagnostics = diagnostics.all();
    return result;
}

bool Assembler::assemble(const std::string& inputPath, const std::string& outputPrefix) {
    std::string sourceText;
    {
        std::ifstream in(inputPath);
        if (!in) {
            std::cerr << "error: cannot open source file: " << inputPath << "\n";
            return false;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        sourceText = buf.str();
    }

    AssembleResult result = assembleText(sourceText);

    if (!result.success) {
        for (const auto& d : result.diagnostics)
            std::cerr << inputPath << ":" << d.lineNumber << ": error: " << d.message << "\n";
        std::cerr << result.diagnostics.size() << " error(s), no output written.\n";
        return false;
    }

    std::ofstream hex(outputPrefix + ".hex");
    for (const auto& e : result.words) {
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", e.machineWord);
        hex << buf << "\n";
    }

    std::ofstream bin(outputPrefix + ".bin", std::ios::binary);
    for (const auto& e : result.words) {
        unsigned char bytes[4] = {
            (unsigned char)((e.machineWord >> 24) & 0xFF),
            (unsigned char)((e.machineWord >> 16) & 0xFF),
            (unsigned char)((e.machineWord >> 8) & 0xFF),
            (unsigned char)(e.machineWord & 0xFF),
        };
        bin.write((char*)bytes, 4);
    }

    std::ofstream sym(outputPrefix + ".sym");
    for (const auto& [name, addr] : result.symbols) {
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", addr);
        sym << buf << " " << name << "\n";
    }

    std::cout << "assembled " << result.words.size() << " instruction(s) -> "
              << outputPrefix << ".hex, " << outputPrefix << ".bin, " << outputPrefix << ".sym\n";
    return true;
}

} // namespace risc201