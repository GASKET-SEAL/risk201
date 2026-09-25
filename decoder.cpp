#include "../include/decoder.hpp"
#include <string>

namespace risc201 {

static int32_t signExtend(uint32_t value, int bits) {
    uint32_t mask = 1u << (bits - 1);
    return (int32_t)((value ^ mask) - mask);
}

static std::string reg(int n) { return "r" + std::to_string(n); }

Decoder::Decoder(const OpcodeTable& opcodeTable) : opcodeTable(opcodeTable) {}

DecodedInstruction Decoder::decode(uint32_t word, uint32_t address) const {
    DecodedInstruction d;
    d.address = address;
    d.word = word;

    uint32_t opcode = (word >> 27) & 0x1F;
    auto info = opcodeTable.lookup(opcode);
    if (!info) {
        d.mnemonic = "dw";
        d.operandText = "0x" + [&]{ char buf[9]; snprintf(buf, sizeof(buf), "%08x", word); return std::string(buf); }();
        return d;
    }

    d.mnemonic = info->mnemonic;

    uint32_t I = (word >> 26) & 0x1;
    uint32_t rd = (word >> 22) & 0xF;
    uint32_t rs1 = (word >> 18) & 0xF;
    uint32_t low18 = word & 0x3FFFF;
    uint32_t rs2 = (low18 >> 14) & 0xF;
    int32_t imm = signExtend(low18, 18);

    switch (info->format) {
    case Format::NO_OPERAND:
        break;
    case Format::BRANCH: {
        uint32_t offsetRaw = word & 0x7FFFFFF;
        int32_t offsetWords = signExtend(offsetRaw, 27);
        int64_t target = (int64_t)(address + 4) + (int64_t)offsetWords * 4;
        d.isBranchLike = true;
        d.branchTargetAddress = (uint32_t)target;
        d.operandText = "<label>";
        break;
    }
    case Format::REG3:
        d.operandText = reg(rd) + ", " + reg(rs1) + ", " + (I ? std::to_string(imm) : reg(rs2));
        break;
    case Format::REG2_NO_RS1:
        d.operandText = reg(rd) + ", " + (I ? std::to_string(imm) : reg(rs2));
        break;
    case Format::REG2_NO_RD:
        d.operandText = reg(rs1) + ", " + (I ? std::to_string(imm) : reg(rs2));
        break;
    case Format::MEM:
        d.operandText = reg(rd) + ", " + std::to_string(imm) + "[" + reg(rs1) + "]";
        break;
    }

    return d;
}

}
