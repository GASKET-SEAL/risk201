#include "../include/assembler_pass1.hpp"

namespace risc201 {

AssemblerPass1::AssemblerPass1(const OpcodeTable& opcodeTable, SymbolTable& symbols, Diagnostics& diagnostics)
    : opcodeTable(opcodeTable), symbols(symbols), diagnostics(diagnostics) {}

std::vector<IntermediateRecord> AssemblerPass1::run(const std::vector<SourceLine>& source) {
    std::vector<IntermediateRecord> out;
    uint32_t addr = 0;

    for (const SourceLine& line : source) {
        if (line.empty) continue;

        if (line.hasLabel) {
            if (symbols.isDefined(line.label)) {
                diagnostics.error(line.lineNumber, "duplicate label '" + line.label + "'");
            } else {
                symbols.define(line.label, addr);
            }
        }

        if (!line.hasMnemonic) continue;

        if (!opcodeTable.exists(line.mnemonic)) {
            diagnostics.error(line.lineNumber, "unknown mnemonic '" + line.mnemonic + "'");
            continue;
        }

        out.push_back({line.lineNumber, addr, line.mnemonic, line.operandText});
        addr += 4;
    }

    return out;
}

} // namespace risc201
