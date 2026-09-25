#pragma once
#include "diagnostics.hpp"
#include "opcode_table.hpp"
#include "symbol_table.hpp"
#include "types.hpp"
#include <vector>

namespace risc201 {

class AssemblerPass1 {
public:
    AssemblerPass1(const OpcodeTable& opcodeTable, SymbolTable& symbols, Diagnostics& diagnostics);
    std::vector<IntermediateRecord> run(const std::vector<SourceLine>& source);

private:
    const OpcodeTable& opcodeTable;
    SymbolTable& symbols;
    Diagnostics& diagnostics;
};

} // namespace risc201
