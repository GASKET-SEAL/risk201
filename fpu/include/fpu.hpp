#pragma once
#include <cstdint>
#include "../../alu/include/units.hpp"

namespace risc201 {

enum class FpuOp { FADD, FSUB, FMUL, FDIV, FCMP, ITOF, FTOI };
enum class RoundMode { Nearest, Zero, Up, Down };
enum class FDivKind { Restoring, Newton, Goldschmidt };

struct FpuConfig {
    RoundMode round = RoundMode::Nearest;
    FDivKind div = FDivKind::Restoring;
    MulKind mul = MulKind::ShiftAdd;      // ShiftAdd = iterative, Booth4 = Booth radix-4
    AdderKind adder = AdderKind::Ripple;  // integer adder used for significand math
};

// IEEE 754 exception flags (accumulate until clearStatus()).
struct FpuFlags {
    bool invalid = false, divzero = false, overflow = false, underflow = false, inexact = false;
};

struct FpuResult {
    uint32_t value = 0;          // result bits (0 for FCMP)
    bool E = false, GT = false;  // FCMP outputs, same meaning as the ALU's E / GT flags
};

class Fpu {
public:
    static constexpr uint32_t QNAN = 0x7FC00000u;

    explicit Fpu(FpuConfig c = FpuConfig()) : cfg(c) {}

    FpuResult execute(FpuOp op, uint32_t a, uint32_t b);  // ITOF/FTOI read operand b

    uint32_t add(uint32_t a, uint32_t b);
    uint32_t sub(uint32_t a, uint32_t b);
    uint32_t mul(uint32_t a, uint32_t b);
    uint32_t div(uint32_t a, uint32_t b);
    FpuResult compare(uint32_t a, uint32_t b);
    uint32_t from_int(int32_t v);
    int32_t  to_int(uint32_t f);

    const FpuFlags& status() const { return flags; }
    void clearStatus() { flags = FpuFlags(); }
    const FpuConfig& config() const { return cfg; }

private:
    uint32_t roundAndPack(bool sign, int E, uint64_t W, int point, bool sticky);
    uint64_t add64(uint64_t x, uint64_t y, bool cin = false) const;
    uint64_t mulSig(uint32_t a, uint32_t b) const;
    uint64_t divRestoring(uint64_t num, uint32_t d, uint64_t& rem) const;
    uint64_t divByReciprocal(uint64_t num, uint32_t n, uint32_t d, uint64_t& rem) const;

    FpuConfig cfg;
    FpuFlags flags;
};

} // namespace risc201
