#include "../include/alu.hpp"

namespace risc201 {

AluResult Alu::doAdd(int32_t a, int32_t b) const {
    AluResult r;
    r.value = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
    return r;
}

AluResult Alu::doSub(int32_t a, int32_t b) const {
    AluResult r;
    r.value = static_cast<uint32_t>(a) - static_cast<uint32_t>(b);
    return r;
}

AluResult Alu::doCmp(int32_t a, int32_t b) const {
    // "cmp = subtract, flags-only" -- the adder still runs the subtraction,
    // but per the design doc the result is discarded (isWb is gated off for
    // cmp), so value is left at 0. Only E/GT are meaningful here.
    AluResult r;
    r.value = 0;
    r.flags.E = (a == b);
    r.flags.GT = (a > b);
    return r;
}

AluResult Alu::doShift(AluOp op, int32_t a, int32_t b) const {
    // "shift amount from op2/immx" -- shift amount comes from B, value being
    // shifted is A. Masked to 5 bits (0-31) since this is a 32-bit barrel
    // shifter and a raw immx could otherwise carry a huge sign-extended value.
    AluResult r;
    uint32_t shamt = static_cast<uint32_t>(b) & 0x1Fu;
    uint32_t ua = static_cast<uint32_t>(a);

    switch (op) {
    case AluOp::LSL:
        r.value = ua << shamt;
        break;
    case AluOp::LSR:
        r.value = ua >> shamt; // logical: zero-fill, bit pattern only
        break;
    case AluOp::ASR: {
        // Arithmetic: sign-fill. Done manually (not relying on how the
        // compiler treats `>>` on a negative int32_t) so behavior is
        // guaranteed rather than implementation-defined.
        if (shamt == 0) {
            r.value = ua;
        } else if (a < 0) {
            uint32_t signFill = shamt == 32 ? 0xFFFFFFFFu : ~((1u << (32 - shamt)) - 1u);
            r.value = (ua >> shamt) | signFill;
        } else {
            r.value = ua >> shamt;
        }
        break;
    }
    default:
        break; // unreachable; op is always one of the three shift ops here
    }
    return r;
}

AluResult Alu::execute(AluOp op, int32_t a, int32_t b, AluFlags currentFlags) const {
    AluResult r;

    switch (op) {
    case AluOp::ADD:
        r = doAdd(a, b);
        break;
    case AluOp::SUB:
        r = doSub(a, b);
        break;
    case AluOp::MUL:
        // Separate block, not adder-derived (per design doc). 32-bit result,
        // truncated on overflow -- same as any fixed-width ALU multiplier.
        r.value = static_cast<uint32_t>(a) * static_cast<uint32_t>(b);
        break;
    case AluOp::DIV:
        if (b == 0) throw AluException("division by zero (div)");
        r.value = static_cast<uint32_t>(a / b);
        break;
    case AluOp::MOD:
        if (b == 0) throw AluException("division by zero (mod)");
        r.value = static_cast<uint32_t>(a % b);
        break;
    case AluOp::AND:
        r.value = static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
        break;
    case AluOp::OR:
        r.value = static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
        break;
    case AluOp::NOT:
        // REG2_NO_RS1: "not" ignores rs1 -- operates on B (rs2/imm) only, `a` unused.
        r.value = ~static_cast<uint32_t>(b);
        break;
    case AluOp::LSL:
    case AluOp::LSR:
    case AluOp::ASR:
        r = doShift(op, a, b);
        break;
    case AluOp::MOV:
        // REG2_NO_RS1: "mov" ignores rs1 -- passes B straight through, `a` unused.
        r.value = static_cast<uint32_t>(b);
        break;
    case AluOp::CMP:
        r = doCmp(a, b);
        return r; // CMP is the one op allowed to change flags -- return its own
    }

    // Every non-CMP op leaves flags exactly as it found them.
    r.flags = currentFlags;
    return r;
}

const char* Alu::toString(AluOp op) {
    switch (op) {
    case AluOp::ADD: return "ADD";
    case AluOp::SUB: return "SUB";
    case AluOp::MUL: return "MUL";
    case AluOp::DIV: return "DIV";
    case AluOp::MOD: return "MOD";
    case AluOp::AND: return "AND";
    case AluOp::OR:  return "OR";
    case AluOp::NOT: return "NOT";
    case AluOp::LSL: return "LSL";
    case AluOp::LSR: return "LSR";
    case AluOp::ASR: return "ASR";
    case AluOp::MOV: return "MOV";
    case AluOp::CMP: return "CMP";
    }
    return "?";
}

AluOp Alu::mnemonicToAluOp(const std::string& mnemonic) {
    if (mnemonic == "add") return AluOp::ADD;
    if (mnemonic == "sub") return AluOp::SUB;
    if (mnemonic == "mul") return AluOp::MUL;
    if (mnemonic == "div") return AluOp::DIV;
    if (mnemonic == "mod") return AluOp::MOD;
    if (mnemonic == "and") return AluOp::AND;
    if (mnemonic == "or")  return AluOp::OR;
    if (mnemonic == "not") return AluOp::NOT;
    if (mnemonic == "mov") return AluOp::MOV;
    if (mnemonic == "lsl") return AluOp::LSL;
    if (mnemonic == "lsr") return AluOp::LSR;
    if (mnemonic == "asr") return AluOp::ASR;
    if (mnemonic == "cmp") return AluOp::CMP;
    throw std::invalid_argument(
        "mnemonic '" + mnemonic + "' does not map to an AluOp "
        "(ld/st use ADD directly for address calc; nop/beq/bgt/b/call/ret never reach the ALU)");
}

} // namespace risc201
