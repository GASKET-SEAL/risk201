#include "../include/alu.hpp"
#include <limits>

namespace risc201 {

namespace {

AluNzcv toNzcv(const AdderOut& s) {
    AluNzcv n;
    n.N = (s.sum >> 31) != 0;
    n.Z = (s.sum == 0);
    n.C = s.carry;
    n.V = s.overflow;
    return n;
}

AluNzcv referenceNzcv(int32_t a, int32_t b, bool sub) {
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t res = sub ? ua - ub : ua + ub;
    int64_t wide = sub ? static_cast<int64_t>(a) - b : static_cast<int64_t>(a) + b;
    AluNzcv n;
    n.N = (res >> 31) != 0;
    n.Z = (res == 0);
    n.V = wide < std::numeric_limits<int32_t>::min() || wide > std::numeric_limits<int32_t>::max();
    n.C = sub ? (ua >= ub) : ((static_cast<uint64_t>(ua) + ub) > 0xFFFFFFFFull);
    return n;
}

} // namespace

Alu::Alu(AluConfig config) : cfg(config) {}

const AluConfig& Alu::config() const { return cfg; }

AluResult Alu::execute(AluOp op, int32_t a, int32_t b, AluFlags currentFlags) const {
    if (b == 0 && op == AluOp::DIV) throw AluException("division by zero (div)");
    if (b == 0 && op == AluOp::MOD) throw AluException("division by zero (mod)");
    if (cfg.reference) return executeReference(op, a, b, currentFlags);
    return executeStructural(op, static_cast<Word>(a), static_cast<Word>(b), currentFlags);
}

AluResult Alu::executeStructural(AluOp op, Word a, Word b, AluFlags flags) const {
    AluResult r;
    r.flags = flags;

    switch (op) {
    case AluOp::ADD: {
        AdderOut s = adderAdd(cfg.adder, a, b);
        r.value = s.sum;
        r.nzcv = toNzcv(s);
        break;
    }
    case AluOp::SUB: {
        AdderOut s = adderSub(cfg.adder, a, b);
        r.value = s.sum;
        r.nzcv = toNzcv(s);
        break;
    }
    case AluOp::CMP: {
        AdderOut s = adderSub(cfg.adder, a, b);
        r.nzcv = toNzcv(s);
        r.flags.E = r.nzcv.Z;
        r.flags.GT = !r.nzcv.Z && (r.nzcv.N == r.nzcv.V);
        break;
    }
    case AluOp::MUL:
        r.value = multiply(cfg.mul, cfg.adder, a, b);
        break;
    case AluOp::DIV:
        r.value = divideSigned(cfg.div, cfg.adder, a, b).quotient;
        break;
    case AluOp::MOD:
        r.value = divideSigned(cfg.div, cfg.adder, a, b).remainder;
        break;
    case AluOp::AND:
        r.value = a & b;
        break;
    case AluOp::OR:
        r.value = a | b;
        break;
    case AluOp::NOT:
        r.value = ~b;
        break;
    case AluOp::LSL:
        r.value = barrelShift(ShiftKind::LSL, a, b);
        break;
    case AluOp::LSR:
        r.value = barrelShift(ShiftKind::LSR, a, b);
        break;
    case AluOp::ASR:
        r.value = barrelShift(ShiftKind::ASR, a, b);
        break;
    case AluOp::MOV:
        r.value = b;
        break;
    }
    return r;
}

AluResult Alu::executeReference(AluOp op, int32_t a, int32_t b, AluFlags flags) const {
    AluResult r;
    r.flags = flags;
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t sh = ub & 0x1Fu;
    bool minOverMinusOne = (a == std::numeric_limits<int32_t>::min() && b == -1);

    switch (op) {
    case AluOp::ADD:
        r.value = ua + ub;
        r.nzcv = referenceNzcv(a, b, false);
        break;
    case AluOp::SUB:
        r.value = ua - ub;
        r.nzcv = referenceNzcv(a, b, true);
        break;
    case AluOp::CMP:
        r.nzcv = referenceNzcv(a, b, true);
        r.flags.E = (a == b);
        r.flags.GT = (a > b);
        break;
    case AluOp::MUL:
        r.value = ua * ub;
        break;
    case AluOp::DIV:
        r.value = minOverMinusOne ? ua : static_cast<uint32_t>(a / b);
        break;
    case AluOp::MOD:
        r.value = minOverMinusOne ? 0u : static_cast<uint32_t>(a % b);
        break;
    case AluOp::AND:
        r.value = ua & ub;
        break;
    case AluOp::OR:
        r.value = ua | ub;
        break;
    case AluOp::NOT:
        r.value = ~ub;
        break;
    case AluOp::LSL:
        r.value = ua << sh;
        break;
    case AluOp::LSR:
        r.value = ua >> sh;
        break;
    case AluOp::ASR:
        r.value = (ua >> sh) | ((a < 0 && sh != 0) ? (0xFFFFFFFFu << (32 - sh)) : 0u);
        break;
    case AluOp::MOV:
        r.value = ub;
        break;
    }
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
