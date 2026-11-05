// Compares the FPU against the host's hardware floats (special values + random operands),
// for every rounding mode and every divider / multiplier variant.
#include "../include/fpu.hpp"
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>
#include <vector>

using namespace risc201;

static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float flt(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
static bool same(uint32_t got, uint32_t want) {   // any NaN == any NaN, otherwise bit-exact
    return (std::isnan(flt(got)) && std::isnan(flt(want))) || got == want;
}

static uint32_t rng = 12345;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main() {
    std::vector<uint32_t> v = {
        0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3FC00000, 0x40400000, 0x3DCCCCCD,
        0x00000001, 0x007FFFFF, 0x00800000, 0x80000003, 0x7F7FFFFF, 0xFF7FFFFF,
        0x7F800000, 0xFF800000, 0x7FC00000, 0x4B800000, 0x4B7FFFFF, 0x1E3CE508, 0x71000000};
    for (int i = 0; i < 300; ++i) v.push_back(rnd());                                   // any bits
    for (int i = 0; i < 700; ++i) v.push_back((rnd() & 0x807FFFFF) | ((100 + rnd() % 56) << 23)); // sane exps

    const int fe[4] = {FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD};
    long checks = 0, fails = 0;
    auto check = [&](const char* what, uint32_t a, uint32_t b, uint32_t got, uint32_t want) {
        ++checks;
        if (!same(got, want) && ++fails <= 10)
            std::printf("FAIL %s a=%08x b=%08x got=%08x want=%08x\n", what, a, b, got, want);
    };

    for (int m = 0; m < 4; ++m) {
        std::fesetround(fe[m]);
        for (int dk = 0; dk < 3; ++dk)
        for (int mk = 0; mk < 2; ++mk) {
            FpuConfig c;
            c.round = RoundMode(m);
            c.div = FDivKind(dk);
            c.mul = MulKind(mk);
            c.adder = AdderKind((dk + mk) % 3);          // also exercise all three adders
            Fpu f(c);
            for (uint32_t a : v) for (uint32_t b : v) {
                volatile float x = flt(a), y = flt(b);
                check("fadd", a, b, f.add(a, b), bits(x + y));
                check("fsub", a, b, f.sub(a, b), bits(x - y));
                check("fmul", a, b, f.mul(a, b), bits(x * y));
                check("fdiv", a, b, f.div(a, b), bits(x / y));
            }
            for (uint32_t a : v) {                       // itof on raw integers
                volatile int32_t i = int32_t(a);
                volatile float r = float(i);
                check("itof", a, 0, f.from_int(int32_t(a)), bits(r));
            }
        }
    }

    std::fesetround(FE_TONEAREST);
    Fpu f;
    for (uint32_t a : v) {
        float x = flt(a);
        int32_t want = std::isnan(x) || x >= 2147483648.0f ? INT_MAX : x < -2147483648.0f ? INT_MIN : int32_t(x);
        check("ftoi", a, 0, uint32_t(f.to_int(a)), uint32_t(want));
        for (uint32_t b : v) {
            FpuResult r = f.compare(a, b);
            float y = flt(b);
            ++checks;
            if (r.E != (x == y) || r.GT != (x > y)) ++fails;
        }
    }
    // a few hand-checked results
    check("1.5+1.5",  0, 0, f.add(0x3FC00000, 0x3FC00000), 0x40400000);
    check("3/2",      0, 0, f.div(0x40400000, 0x40000000), 0x3FC00000);
    check("itof -7",  0, 0, f.from_int(-7), 0xC0E00000);
    check("ftoi -7.9", 0, 0, uint32_t(f.to_int(0xC0FD999A)), uint32_t(-7));

    std::printf("%ld checks, %ld failures\n", checks, fails);
    return fails ? 1 : 0;
}
