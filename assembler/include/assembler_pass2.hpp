#pragma once
#include "diagnostics.hpp"
#include "opcode_table.hpp"
#include "symbol_table.hpp"
#include "types.hpp"
#include <vector>

namespace risc201 {

class AssemblerPass2 {
public:
    AssemblerPass2(const OpcodeTable& opcodeTable, const SymbolTable& symbols, Diagnostics& diagnostics);
    std::vector<EncodedRecord> run(const std::vector<IntermediateRecord>& records);

private:
    const OpcodeTable& opcodeTable;
    const SymbolTable& symbols;
    Diagnostics& diagnostics;

    uint32_t encodeInstruction(const IntermediateRecord& r, const OpcodeInfo& info);
};

} // namespace risc201
