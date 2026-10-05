#pragma once
#include "diagnostics.hpp"
#include "opcode_table.hpp"
#include "parser.hpp"
#include "types.hpp"
#include <string>

namespace risc201 {

class Assembler {
public:
    AssembleResult assembleText(const std::string& sourceText);
    bool assemble(const std::string& inputPath, const std::string& outputPrefix);

private:
    Parser parser;
    OpcodeTable opcodeTable;
};

} // namespace risc201