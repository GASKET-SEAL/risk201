#pragma once
#include "diagnostics.hpp"
#include "opcode_table.hpp"
#include "parser.hpp"
#include "symbol_table.hpp"
#include <string>

namespace risc201 {

class Assembler {
public:
    bool assemble(const std::string& inputPath, const std::string& outputPrefix);

private:
    Parser parser;
    OpcodeTable opcodeTable;
    SymbolTable symbols;
    Diagnostics diagnostics;
};

} // namespace risc201
