#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace risc201 {

// One-hot control signal, per Part 1 design doc section 7 ("EX Stage -- Inside
// the ALU"). Six sub-units feed one result mux -> aluResult:
//   Adder      : ADD, SUB, CMP        (and, from the CPU side, ld/st base+offset -- see note below)
//   Multiplier : MUL
//   Divider    : DIV, MOD
//   Logical    : AND, OR, NOT
//   Shift      : LSL, LSR, ASR
//   Mov        : MOV
//
// ld/st do NOT get their own AluOp. Per the design doc, "MA: isLd/isSt drive
// the memory unit (mar=aluResult as address ...)" -- they reuse the Adder for
// base+offset address calculation. The future CPU computes that address with
// execute(AluOp::ADD, rs1Value, immx, flags) and does not touch flags with it.
enum class AluOp {
    ADD, SUB, MUL, DIV, MOD,
    AND, OR, NOT,
    LSL, LSR, ASR,
    MOV, CMP
};

// E (equal) / GT (greater-than). Per the design doc these are written ONLY by
// cmp's adder path; every other ALU op must leave them exactly as it found
// them. That is why execute() takes the current flags in and returns
// (possibly unchanged) flags out, rather than the Alu owning a flags register
// itself -- flag storage belongs to the future CPU/register-file, not here.
struct AluFlags {
    bool E = false;
    bool GT = false;
};

struct AluResult {
    uint32_t value = 0;   // aluResult -- meaningless for CMP (isWb is gated off for cmp anyway)
    AluFlags flags;       // updated only when op == CMP; otherwise identical to the flags passed in
};

// Thrown for a genuine runtime fault (currently: division/modulo by zero).
// Deliberately NOT collected like Diagnostics in the assembler -- that class
// handles static assemble-time errors; this is a runtime fault during
// execution, meant to be caught by the CPU simulator's exception framework
// (assignment item 2f) once that exists.
class AluException : public std::runtime_error {
public:
    explicit AluException(const std::string& msg) : std::runtime_error(msg) {}
};

class Alu {
public:
    // a = op1 (R[rs1]), b = op2 (R[rs2] or immx per the I-bit) -- matches the
    // design doc's "EX: ALU: A = op1, B = isImmediate ? immx : op2" exactly,
    // so the future CPU can wire register-file outputs straight in.
    AluResult execute(AluOp op, int32_t a, int32_t b, AluFlags currentFlags) const;

    static const char* toString(AluOp op);

    // Maps an assembler/disassembler mnemonic to its AluOp, for the future
    // CPU to go straight from OpcodeTable lookups to ALU calls. Only covers
    // mnemonics that actually go through the ALU -- nop/ld/st/beq/bgt/b/
    // call/ret are NOT valid inputs (ld/st use ADD directly for address calc;
    // branches/nop/ret never reach the ALU at all, per the design doc's
    // separate branch-taken logic) and throw std::invalid_argument.
    static AluOp mnemonicToAluOp(const std::string& mnemonic);

private:
    AluResult doAdd(int32_t a, int32_t b) const;
    AluResult doSub(int32_t a, int32_t b) const;
    AluResult doCmp(int32_t a, int32_t b) const;
    AluResult doShift(AluOp op, int32_t a, int32_t b) const;
};

} // namespace risc201
