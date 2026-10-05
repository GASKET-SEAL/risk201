#include "../include/alu.hpp"
#include "../include/units.hpp"
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

using namespace risc201;

static long checks = 0;
static long failures = 0;

static const AluOp kOps[] = {
    AluOp::ADD, AluOp::SUB, AluOp::MUL, AluOp::DIV, AluOp::MOD, AluOp::AND, AluOp::OR,
    AluOp::NOT, AluOp::LSL, AluOp::LSR, AluOp::ASR, AluOp::MOV, AluOp::CMP};

static const AdderKind kAdders[] = {AdderKind::Ripple, AdderKind::Lookahead, AdderKind::KoggeStone};
static const MulKind kMuls[] = {MulKind::ShiftAdd, MulKind::Booth4};
static const DivKind kDivs[] = {DivKind::Restoring, DivKind::NonRestoring};

static std::vector<int32_t> edgeValues() {
    return {0, 1, -1, 2, -2, 3, 5, 7, -7, 10, 17, -17, 31, 32, 33, 36,
            0x7FFFFFFF, 0x7FFFFFFE, std::numeric_limits<int32_t>::min(),
            std::numeric_limits<int32_t>::min() + 1, 0x55555555,
            static_cast<int32_t>(0xAAAAAAAAu), 0x10000, 0xFFFF, 0x8000};
}

static bool sameResult(const AluResult& x, const AluResult& y) {
    return x.value == y.value && x.flags.E == y.flags.E && x.flags.GT == y.flags.GT &&
           x.nzcv.N == y.nzcv.N && x.nzcv.Z == y.nzcv.Z && x.nzcv.C == y.nzcv.C &&
           x.nzcv.V == y.nzcv.V;
}

static void compareOne(const Alu& ref, const Alu& dut, AluOp op, int32_t a, int32_t b, AluFlags f) {
    AluResult rr, dr;
    bool refThrew = false, dutThrew = false;
    try { rr = ref.execute(op, a, b, f); } catch (const AluException&) { refThrew = true; }
    try { dr = dut.execute(op, a, b, f); } catch (const AluException&) { dutThrew = true; }
    ++checks;
    bool ok = (refThrew == dutThrew) && (refThrew || sameResult(rr, dr));
    if (!ok) {
        ++failures;
        if (failures <= 20) {
            std::printf("FAIL %s a=0x%08x b=0x%08x ref=0x%08x dut=0x%08x [%s/%s/%s]\n",
                        Alu::toString(op), static_cast<uint32_t>(a), static_cast<uint32_t>(b),
                        rr.value, dr.value, toString(dut.config().adder),
                        toString(dut.config().mul), toString(dut.config().div));
        }
    }
}

static void testAdders(std::mt19937& rng) {
    std::vector<int32_t> edge = edgeValues();
    for (AdderKind kind : kAdders) {
        long bad = 0;
        long n = 0;
        for (int i = 0; i < 300000; ++i) {
            uint32_t a = (i % 3 == 0) ? static_cast<uint32_t>(edge[rng() % edge.size()]) : rng();
            uint32_t b = (i % 5 == 0) ? static_cast<uint32_t>(edge[rng() % edge.size()]) : rng();
            bool cin = (rng() & 1) != 0;

            uint64_t wide = static_cast<uint64_t>(a) + b + (cin ? 1 : 0);
            int64_t sw = static_cast<int64_t>(static_cast<int32_t>(a)) + static_cast<int32_t>(b) + (cin ? 1 : 0);
            AdderOut s = adderAdd(kind, a, b, cin);
            bool addOk = s.sum == static_cast<uint32_t>(wide) && s.carry == ((wide >> 32) != 0) &&
                         s.overflow == (sw < std::numeric_limits<int32_t>::min() || sw > std::numeric_limits<int32_t>::max());

            int64_t dw = static_cast<int64_t>(static_cast<int32_t>(a)) - static_cast<int32_t>(b);
            AdderOut d = adderSub(kind, a, b);
            bool subOk = d.sum == a - b && d.carry == (a >= b) &&
                         d.overflow == (dw < std::numeric_limits<int32_t>::min() || dw > std::numeric_limits<int32_t>::max());

            n += 2;
            if (!addOk) ++bad;
            if (!subOk) ++bad;
        }
        checks += n;
        failures += bad;
        std::printf("adder %-10s %8ld checks %s\n", toString(kind), n, bad == 0 ? "PASS" : "FAIL");
    }
}

int main() {
    std::mt19937 rng(20260930);
    std::vector<int32_t> edge = edgeValues();

    testAdders(rng);

    AluConfig refCfg;
    refCfg.reference = true;
    Alu ref(refCfg);

    for (AdderKind ak : kAdders) {
        for (MulKind mk : kMuls) {
            for (DivKind dk : kDivs) {
                AluConfig cfg;
                cfg.adder = ak;
                cfg.mul = mk;
                cfg.div = dk;
                Alu dut(cfg);

                long before = failures;
                long checksBefore = checks;

                for (int32_t a : edge)
                    for (int32_t b : edge)
                        for (AluOp op : kOps) {
                            AluFlags f;
                            f.E = (rng() & 1) != 0;
                            f.GT = (rng() & 1) != 0;
                            compareOne(ref, dut, op, a, b, f);
                        }

                for (int i = 0; i < 25000; ++i) {
                    int32_t a = static_cast<int32_t>(rng());
                    int32_t b;
                    switch (rng() % 4) {
                    case 0:  b = static_cast<int32_t>(rng()); break;
                    case 1:  b = static_cast<int32_t>(rng() % 64) - 32; break;
                    case 2:  b = edge[rng() % edge.size()]; break;
                    default: b = static_cast<int32_t>(rng() >> (rng() % 32)); break;
                    }
                    for (AluOp op : kOps) {
                        AluFlags f;
                        f.E = (rng() & 1) != 0;
                        f.GT = (rng() & 1) != 0;
                        compareOne(ref, dut, op, a, b, f);
                    }
                }

                std::printf("alu   %-10s %-8s %-12s %8ld checks %s\n", toString(ak), toString(mk),
                            toString(dk), checks - checksBefore, failures == before ? "PASS" : "FAIL");
            }
        }
    }

    std::printf("\n%ld checks, %ld failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
