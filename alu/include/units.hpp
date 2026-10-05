#pragma once
#include <cstdint>

namespace risc201 {

using Word = uint32_t;

enum class AdderKind { Ripple, Lookahead, KoggeStone };
enum class MulKind { ShiftAdd, Booth4 };
enum class DivKind { Restoring, NonRestoring };
enum class ShiftKind { LSL, LSR, ASR };

struct AdderOut {
    Word sum;
    bool carry;
    bool overflow;
};

struct DivOut {
    Word quotient;
    Word remainder;
};

AdderOut adderAdd(AdderKind kind, Word a, Word b, bool cin = false);
AdderOut adderSub(AdderKind kind, Word a, Word b);
Word negate(AdderKind kind, Word a);

Word multiply(MulKind kind, AdderKind adder, Word a, Word b);

DivOut divideSigned(DivKind kind, AdderKind adder, Word a, Word b);

Word barrelShift(ShiftKind kind, Word value, Word amount);

const char* toString(AdderKind kind);
const char* toString(MulKind kind);
const char* toString(DivKind kind);

} // namespace risc201
