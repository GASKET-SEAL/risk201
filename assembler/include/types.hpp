#pragma once
#include <cstdint>
#include <string>

namespace risc201 {

enum class Format { NO_OPERAND, BRANCH, REG3, REG2_NO_RS1, REG2_NO_RD, MEM };

struct SourceLine {
    int lineNumber = 0;
    bool empty = true;
    std::string label;
    bool hasLabel = false;
    std::string mnemonic;
    bool hasMnemonic = false;
    std::string operandText;
};

struct OpcodeInfo {
    uint32_t opcode = 0;
    Format format = Format::NO_OPERAND;
};

struct IntermediateRecord {
    int lineNumber = 0;
    uint32_t address = 0;
    std::string mnemonic;
    std::string operandText;
};

struct EncodedRecord {
    int lineNumber = 0;
    uint32_t address = 0;
    uint32_t machineWord = 0;
};

} // namespace risc201
