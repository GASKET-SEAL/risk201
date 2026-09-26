#pragma once
#include <cstdint>
#include <string>

namespace risc201 {

enum class Format { NO_OPERAND, BRANCH, REG3, REG2_NO_RS1, REG2_NO_RD, MEM };

struct OpcodeInfo {
    std::string mnemonic;
    Format format;
};

struct DecodedInstruction {
    uint32_t address = 0;
    uint32_t word = 0;
    std::string mnemonic;
    std::string operandText;
    bool isBranchLike = false;
    bool branchTargetInRange = false;
    uint32_t branchTargetAddress = 0;
};

}
