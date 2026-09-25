#include "../include/assembler_pass2.hpp"
#include "../include/utilities.hpp"

namespace risc201 {

static uint32_t packRegForm(uint32_t opcode, int I, int rd, int rs1, int lowField18) {
    uint32_t w = 0;
    w |= (opcode & 0x1Fu) << 27;
    w |= (uint32_t(I) & 0x1u) << 26;
    w |= (uint32_t(rd) & 0xFu) << 22;
    w |= (uint32_t(rs1) & 0xFu) << 18;
    w |= (uint32_t(lowField18) & 0x3FFFFu);
    return w;
}

static uint32_t packBranch(uint32_t opcode, int32_t offsetWords) {
    return ((opcode & 0x1Fu) << 27) | (uint32_t(offsetWords) & 0x7FFFFFFu);
}

static uint32_t packNoOperand(uint32_t opcode) {
    return (opcode & 0x1Fu) << 27;
}

AssemblerPass2::AssemblerPass2(const OpcodeTable& opcodeTable, const SymbolTable& symbols, Diagnostics& diagnostics)
    : opcodeTable(opcodeTable), symbols(symbols), diagnostics(diagnostics) {}

uint32_t AssemblerPass2::encodeInstruction(const IntermediateRecord& r, const OpcodeInfo& info) {
    using namespace util;

    switch (info.format) {
    case Format::NO_OPERAND: {
        if (!r.operandText.empty()) diagnostics.error(r.lineNumber, r.mnemonic + " takes no operands");
        return packNoOperand(info.opcode);
    }
    case Format::BRANCH: {
        std::string tgt = trim(r.operandText);
        long targetAddr;
        if (symbols.isDefined(tgt)) {
            targetAddr = symbols.valueOf(tgt);
        } else if (parseImmediate(tgt, targetAddr)) {
        } else {
            diagnostics.error(r.lineNumber, "undefined label '" + tgt + "'");
            targetAddr = r.address + 4;
        }
        long nextAddr = r.address + 4;
        long deltaBytes = targetAddr - nextAddr;
        if (deltaBytes % 4 != 0) {
            diagnostics.error(r.lineNumber, "branch target not 4-byte aligned");
            deltaBytes = 0;
        }
        long offsetWords = deltaBytes / 4;
        if (!fitsSigned(offsetWords, 27))
            diagnostics.error(r.lineNumber, "branch offset out of range");
        return packBranch(info.opcode, (int32_t)offsetWords);
    }
    case Format::REG3: {
        auto ops = splitComma(r.operandText);
        if (ops.size() != 3) {
            diagnostics.error(r.lineNumber, r.mnemonic + " needs 3 operands: rd, rs1, rs2/imm");
            return 0;
        }
        int rd, rs1, rs2;
        if (!isRegisterToken(ops[0], rd)) { diagnostics.error(r.lineNumber, "bad register '" + ops[0] + "'"); return 0; }
        if (!isRegisterToken(ops[1], rs1)) { diagnostics.error(r.lineNumber, "bad register '" + ops[1] + "'"); return 0; }
        if (isRegisterToken(ops[2], rs2)) return packRegForm(info.opcode, 0, rd, rs1, rs2 << 14);
        long imm;
        if (!parseImmediate(ops[2], imm)) { diagnostics.error(r.lineNumber, "bad operand '" + ops[2] + "'"); return 0; }
        if (!fitsSigned(imm, 18)) diagnostics.error(r.lineNumber, "immediate out of range");
        return packRegForm(info.opcode, 1, rd, rs1, (int)imm & 0x3FFFF);
    }
    case Format::REG2_NO_RS1: {
        auto ops = splitComma(r.operandText);
        if (ops.size() != 2) {
            diagnostics.error(r.lineNumber, r.mnemonic + " needs 2 operands: rd, rs2/imm");
            return 0;
        }
        int rd, rs2;
        if (!isRegisterToken(ops[0], rd)) { diagnostics.error(r.lineNumber, "bad register '" + ops[0] + "'"); return 0; }
        if (isRegisterToken(ops[1], rs2)) return packRegForm(info.opcode, 0, rd, 0, rs2 << 14);
        long imm;
        if (!parseImmediate(ops[1], imm)) { diagnostics.error(r.lineNumber, "bad operand '" + ops[1] + "'"); return 0; }
        if (!fitsSigned(imm, 18)) diagnostics.error(r.lineNumber, "immediate out of range");
        return packRegForm(info.opcode, 1, rd, 0, (int)imm & 0x3FFFF);
    }
    case Format::REG2_NO_RD: {
        auto ops = splitComma(r.operandText);
        if (ops.size() != 2) {
            diagnostics.error(r.lineNumber, r.mnemonic + " needs 2 operands: rs1, rs2/imm");
            return 0;
        }
        int rs1, rs2;
        if (!isRegisterToken(ops[0], rs1)) { diagnostics.error(r.lineNumber, "bad register '" + ops[0] + "'"); return 0; }
        if (isRegisterToken(ops[1], rs2)) return packRegForm(info.opcode, 0, 0, rs1, rs2 << 14);
        long imm;
        if (!parseImmediate(ops[1], imm)) { diagnostics.error(r.lineNumber, "bad operand '" + ops[1] + "'"); return 0; }
        if (!fitsSigned(imm, 18)) diagnostics.error(r.lineNumber, "immediate out of range");
        return packRegForm(info.opcode, 1, 0, rs1, (int)imm & 0x3FFFF);
    }
    case Format::MEM: {
        auto ops = splitComma(r.operandText);
        if (ops.size() != 2) {
            diagnostics.error(r.lineNumber, r.mnemonic + " needs 2 operands: rd, imm[rs1]");
            return 0;
        }
        int rd, rs1;
        long imm;
        if (!isRegisterToken(ops[0], rd)) { diagnostics.error(r.lineNumber, "bad register '" + ops[0] + "'"); return 0; }
        if (!parseMemOperand(ops[1], imm, rs1)) { diagnostics.error(r.lineNumber, "bad memory operand '" + ops[1] + "'"); return 0; }
        if (!fitsSigned(imm, 18)) diagnostics.error(r.lineNumber, "offset out of range");
        return packRegForm(info.opcode, 1, rd, rs1, (int)imm & 0x3FFFF);
    }
    }
    return 0;
}

std::vector<EncodedRecord> AssemblerPass2::run(const std::vector<IntermediateRecord>& records) {
    std::vector<EncodedRecord> out;
    for (const IntermediateRecord& r : records) {
        const OpcodeInfo& info = opcodeTable.get(r.mnemonic);
        uint32_t word = encodeInstruction(r, info);
        out.push_back({r.lineNumber, r.address, word});
    }
    return out;
}

} // namespace risc201
