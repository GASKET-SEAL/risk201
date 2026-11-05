#include "../include/fpu.hpp"
#include <utility>

namespace risc201 {

namespace {

using U64 = uint64_t;
constexpr int BIAS = 127;
constexpr uint32_t HIDDEN = 1u << 23, INF = 0x7F800000u, SIGN = 0x80000000u;

struct Unpacked {
    bool sign, nan, snan, inf, zero;
    int exp;        // effective biased exponent (denormals use 1)
    uint32_t man;   // 24-bit significand (hidden 1 included; denormals have none)
};

Unpacked unpack(uint32_t x) {
    Unpacked u;
    int e = (x >> 23) & 0xFF;
    uint32_t f = x & 0x7FFFFF;
    u.sign = (x >> 31) != 0;
    u.nan = (e == 255 && f);
    u.snan = u.nan && !(f & 0x400000);
    u.inf = (e == 255 && !f);
    u.zero = (e == 0 && !f);
    u.exp = e ? e : 1;
    u.man = e ? (f | HIDDEN) : f;
    return u;
}

void normalize(Unpacked& u) {  // denormal -> leading 1 at bit 23 (caller excludes zero)
    while (!(u.man & HIDDEN)) { u.man <<= 1; --u.exp; }
}

int msb(U64 w) { int m = -1; while (w) { w >>= 1; ++m; } return m; }

// Right shift; every bit shifted out is OR-ed into bit 0 (the sticky bit).
U64 stickyShr(U64 w, int n) {
    if (n <= 0) return w;
    if (n >= 64) return w != 0;
    U64 lost = w & ((U64(1) << n) - 1);
    return (w >> n) | (lost != 0);
}

} // namespace

// ---------------------------------------------------------------- helpers

// 64-bit add built from the project's 32-bit integer adder.
U64 Fpu::add64(U64 x, U64 y, bool cin) const {
    AdderOut lo = adderAdd(cfg.adder, Word(x), Word(y), cin);
    AdderOut hi = adderAdd(cfg.adder, Word(x >> 32), Word(y >> 32), lo.carry);
    return (U64(hi.sum) << 32) | lo.sum;
}

// Core shared subroutine. Value = W * 2^-point * 2^(E-BIAS); `sticky` = nonzero bits below W.
uint32_t Fpu::roundAndPack(bool sign, int E, U64 W, int point, bool sticky) {
    if (W == 0) return sign ? SIGN : 0;

    // Normalization: leading 1 -> bit 26 (24 significand bits | G R S).
    int m = msb(W);
    int Elead = E + m - point;                      // biased exponent of the leading 1
    W = (m > 26) ? stickyShr(W, m - 26) : (W << (26 - m));
    if (sticky) W |= 1;

    // Denormal handling: shift further right, exponent field becomes 0.
    bool tiny = false;
    if (Elead < 1) { W = stickyShr(W, 1 - Elead); Elead = 1; tiny = true; }

    // GRS extraction + rounding selection.
    bool G = (W >> 2) & 1, R = (W >> 1) & 1, S = W & 1;
    uint32_t sig = uint32_t(W >> 3);
    bool inexact = G || R || S, up = false;
    switch (cfg.round) {
    case RoundMode::Nearest: up = G && (R || S || (sig & 1)); break;
    case RoundMode::Zero:    break;
    case RoundMode::Up:      up = !sign && inexact; break;
    case RoundMode::Down:    up = sign && inexact; break;
    }
    sig += up;

    // Post-rounding check: carry out (1.11..1 + 1), denormal -> normal, overflow.
    if (sig >> 24) { sig >>= 1; ++Elead; }
    int Ef = (sig & HIDDEN) ? Elead : 0;
    uint32_t s = sign ? SIGN : 0;

    if (Ef >= 255) {
        flags.overflow = flags.inexact = true;
        bool toInf = cfg.round == RoundMode::Nearest ||
                     (cfg.round == RoundMode::Up && !sign) || (cfg.round == RoundMode::Down && sign);
        return s | (toInf ? INF : 0x7F7FFFFFu);
    }
    if (inexact) { flags.inexact = true; if (tiny) flags.underflow = true; }
    return s | (uint32_t(Ef) << 23) | (sig & 0x7FFFFF);
}

// ------------------------------------------------------- add / sub

uint32_t Fpu::sub(uint32_t a, uint32_t b) { return add(a, b ^ SIGN); }

uint32_t Fpu::add(uint32_t a, uint32_t b) {
    Unpacked x = unpack(a), y = unpack(b);

    // 1. Special cases
    if (x.nan || y.nan) { if (x.snan || y.snan) flags.invalid = true; return QNAN; }
    if (x.inf || y.inf) {
        if (x.inf && y.inf && x.sign != y.sign) { flags.invalid = true; return QNAN; }
        return x.inf ? a : b;
    }
    if (x.zero && y.zero) return ((x.sign == y.sign) ? x.sign : cfg.round == RoundMode::Down) ? SIGN : 0;
    if (x.zero) return b;
    if (y.zero) return a;

    // 2. Alignment: swap so x has the larger exponent; shift y right, keeping sticky.
    //    29 spare low bits keep the sticky bit far below G/R after any cancellation.
    if (x.exp < y.exp) std::swap(x, y);
    U64 wa = U64(x.man) << 29;
    U64 wb = stickyShr(U64(y.man) << 29, x.exp - y.exp);

    // 3. Effective subtraction -> two's complement of the aligned significand.
    bool sign = x.sign;
    if (x.sign != y.sign) wb = add64(~wb, 0, true);

    // 4. Significand addition; negative result -> negate and flip the result sign.
    U64 sum = add64(wa, wb);
    if (sum >> 63) { sum = add64(~sum, 0, true); sign = !sign; }
    if (sum == 0) return cfg.round == RoundMode::Down ? SIGN : 0;   // exact cancellation

    // 5. Normalize, round, pack
    return roundAndPack(sign, x.exp, sum, 23 + 29, false);
}

// ------------------------------------------------------- multiplication

// 24x24 -> 48-bit significand product (iterative shift-add, or Booth radix-4).
U64 Fpu::mulSig(uint32_t a, uint32_t b) const {
    U64 acc = 0;
    if (cfg.mul == MulKind::ShiftAdd) {
        for (int i = 0; i < 24; ++i)
            if ((b >> i) & 1) acc = add64(acc, U64(a) << i);
        return acc;
    }
    bool prev = false;
    for (int i = 0; i < 13; ++i) {          // 13 digits cover an unsigned 24-bit multiplier
        bool lo = (b >> (2 * i)) & 1, hi = (b >> (2 * i + 1)) & 1;
        int digit = int(lo) + int(prev) - 2 * int(hi);
        prev = hi;
        U64 pp = U64(a) << (2 * i);
        switch (digit) {
        case 1:  acc = add64(acc, pp); break;
        case 2:  acc = add64(acc, pp << 1); break;
        case -1: acc = add64(acc, ~pp, true); break;
        case -2: acc = add64(acc, ~(pp << 1), true); break;
        default: break;
        }
    }
    return acc;
}

uint32_t Fpu::mul(uint32_t a, uint32_t b) {
    Unpacked x = unpack(a), y = unpack(b);
    bool sign = x.sign != y.sign;                       // S = S_A xor S_B
    uint32_t s = sign ? SIGN : 0;

    if (x.nan || y.nan) { if (x.snan || y.snan) flags.invalid = true; return QNAN; }
    if (x.inf || y.inf) {
        if (x.zero || y.zero) { flags.invalid = true; return QNAN; }   // 0 * inf
        return s | INF;
    }
    if (x.zero || y.zero) return s;

    normalize(x); normalize(y);
    U64 p = mulSig(x.man, y.man);                       // 46 fraction bits
    return roundAndPack(sign, x.exp + y.exp - BIAS, p, 46, false);
}

// ------------------------------------------------------- division

// Restoring division of a 50-bit numerator by a 24-bit divisor (trial subtraction per bit).
U64 Fpu::divRestoring(U64 num, uint32_t d, U64& rem) const {
    Word r = 0;
    U64 q = 0;
    for (int i = 49; i >= 0; --i) {
        r = (r << 1) | Word((num >> i) & 1);
        AdderOut t = adderSub(cfg.adder, r, d);
        if (t.carry) { r = t.sum; q |= U64(1) << i; }
    }
    rem = r;
    return q;
}

// Quotient from a reciprocal: q ~= n * (1/d), then exact remainder correction so the
// quotient and sticky bit are exactly what restoring division would give.
U64 Fpu::divByReciprocal(U64 num, uint32_t n, uint32_t d, U64& rem) const {
    U64 R;   // R ~= 2^54 / d  (1/d as Q.30 with d in [0.5,1) as a fraction)
    if (cfg.div == FDivKind::Newton) {
        // x(n+1) = x(n) * (2 - d*x(n)), error squares each step; linear seed 48/17 - 32/17*d.
        U64 df = U64(d) << 6;                                    // d in Q.30
        R = ((48ull << 30) - 32ull * df) / 17;
        for (int i = 0; i < 4; ++i) {
            U64 e = (df * R) >> 30;
            R = (R * ((2ull << 30) - e)) >> 30;
        }
    } else {
        // Goldschmidt: P = d/2^23 = 1+X, Y = (1-X)/2 = (2^24-d)/2^24, so
        // 1/P = (1/2)(1+Y)(1+Y^2)(1+Y^4)(1+Y^8)(1+Y^16); the 1/2 is absorbed by the exponent.
        U64 t = U64((1u << 24) - d) << 6;                        // Y in Q.30
        R = 1ull << 30;
        for (int k = 0; k < 5; ++k) { R += (R * t) >> 30; t = (t * t) >> 30; }
    }
    U64 q = (U64(n) * R) >> 28;
    int64_t r = int64_t(num) - int64_t(q) * int64_t(d);
    while (r < 0) { --q; r += d; }
    while (r >= int64_t(d)) { ++q; r -= d; }
    rem = U64(r);
    return q;
}

uint32_t Fpu::div(uint32_t a, uint32_t b) {
    Unpacked x = unpack(a), y = unpack(b);
    bool sign = x.sign != y.sign;
    uint32_t s = sign ? SIGN : 0;

    if (x.nan || y.nan) { if (x.snan || y.snan) flags.invalid = true; return QNAN; }
    if ((x.inf && y.inf) || (x.zero && y.zero)) { flags.invalid = true; return QNAN; }
    if (x.inf) return s | INF;
    if (y.inf) return s;
    if (y.zero) { flags.divzero = true; return s | INF; }
    if (x.zero) return s;

    normalize(x); normalize(y);
    U64 num = U64(x.man) << 26, rem = 0, q;
    q = (cfg.div == FDivKind::Restoring) ? divRestoring(num, y.man, rem)
                                         : divByReciprocal(num, x.man, y.man, rem);
    // Q ~ (Ma/Mb) * 2^26; remainder != 0 becomes the sticky bit.
    return roundAndPack(sign, x.exp - y.exp + BIAS, q, 26, rem != 0);
}

// ------------------------------------------------------- compare / convert

FpuResult Fpu::compare(uint32_t a, uint32_t b) {
    FpuResult r;
    if (unpack(a).nan || unpack(b).nan) { flags.invalid = true; return r; }   // unordered: E=GT=0
    // sign-magnitude -> two's complement so integer compare gives float order (+0 == -0)
    auto key = [this](uint32_t v) {
        Word mag = v & 0x7FFFFFFFu;
        return int32_t((v >> 31) ? negate(cfg.adder, mag) : mag);
    };
    int32_t ka = key(a), kb = key(b);
    r.E = (ka == kb);
    r.GT = (ka > kb);
    return r;
}

uint32_t Fpu::from_int(int32_t v) {
    bool neg = v < 0;
    Word mag = neg ? negate(cfg.adder, Word(v)) : Word(v);
    return roundAndPack(neg, BIAS, mag, 0, false);
}

int32_t Fpu::to_int(uint32_t f) {
    Unpacked x = unpack(f);
    if (x.nan) { flags.invalid = true; return INT32_MAX; }
    if (x.zero) return 0;
    int e = x.exp - BIAS;
    if (e < 0) { flags.inexact = true; return 0; }                 // |f| < 1 truncates to 0
    if (e == 31 && x.sign && x.man == HIDDEN) return INT32_MIN;    // exactly -2^31
    if (e >= 31) { flags.invalid = true; return x.sign ? INT32_MIN : INT32_MAX; }   // saturate
    Word v;
    if (e >= 23) v = x.man << (e - 23);
    else {
        v = x.man >> (23 - e);                                     // truncate toward zero
        if (x.man & ((1u << (23 - e)) - 1)) flags.inexact = true;
    }
    return int32_t(x.sign ? negate(cfg.adder, v) : v);
}

FpuResult Fpu::execute(FpuOp op, uint32_t a, uint32_t b) {
    FpuResult r;
    switch (op) {
    case FpuOp::FADD: r.value = add(a, b); break;
    case FpuOp::FSUB: r.value = sub(a, b); break;
    case FpuOp::FMUL: r.value = mul(a, b); break;
    case FpuOp::FDIV: r.value = div(a, b); break;
    case FpuOp::FCMP: r = compare(a, b); break;
    case FpuOp::ITOF: r.value = from_int(int32_t(b)); break;
    case FpuOp::FTOI: r.value = uint32_t(to_int(b)); break;
    }
    return r;
}

} // namespace risc201
