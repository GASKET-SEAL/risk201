#pragma once
#include <cstdint>
#include <map>
#include <string>
#include "diagnostics.hpp"
#include <vector>

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

struct AssembleResult {
    bool success = false;
    std::vector<EncodedRecord> words;
    std::map<std::string, uint32_t> symbols;
    std::vector<Diagnostic> diagnostics;
};

} // namespace risc201
