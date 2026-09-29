#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include "units.hpp"

namespace risc201 {

enum class AluOp {
    ADD, SUB, MUL, DIV, MOD,
    AND, OR, NOT,
    LSL, LSR, ASR,
    MOV, CMP
};

struct AluFlags {
    bool E = false;
    bool GT = false;
};

struct AluNzcv {
    bool N = false;
    bool Z = false;
    bool C = false;
    bool V = false;
};

struct AluResult {
    uint32_t value = 0;
    AluFlags flags;
    AluNzcv nzcv;
};

class AluException : public std::runtime_error {
public:
    explicit AluException(const std::string& msg) : std::runtime_error(msg) {}
};

struct AluConfig {
    bool reference = false;
    AdderKind adder = AdderKind::Ripple;
    MulKind mul = MulKind::ShiftAdd;
    DivKind div = DivKind::Restoring;
};

class Alu {
public:
    explicit Alu(AluConfig config = AluConfig());

    AluResult execute(AluOp op, int32_t a, int32_t b, AluFlags currentFlags) const;
    const AluConfig& config() const;

    static const char* toString(AluOp op);
    static AluOp mnemonicToAluOp(const std::string& mnemonic);

private:
    AluResult executeStructural(AluOp op, Word a, Word b, AluFlags flags) const;
    AluResult executeReference(AluOp op, int32_t a, int32_t b, AluFlags flags) const;

    AluConfig cfg;
};

} // namespace risc201
