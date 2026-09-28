#include "../include/alu.hpp"
#include <cstdio>
#include <cstdint>

using namespace risc201;

static int passCount = 0;
static int failCount = 0;

static void check(const char* name, uint32_t got, uint32_t expected) {
    bool ok = (got == expected);
    ok ? ++passCount : ++failCount;
    std::printf("[%s] %-28s got=0x%08x expected=0x%08x\n", ok ? "PASS" : "FAIL", name, got, expected);
}

static void checkBool(const char* name, bool got, bool expected) {
    bool ok = (got == expected);
    ok ? ++passCount : ++failCount;
    std::printf("[%s] %-28s got=%s expected=%s\n", ok ? "PASS" : "FAIL", name, got ? "true" : "false", expected ? "true" : "false");
}

int main() {
    Alu alu;
    AluFlags flags; // starts {E=false, GT=false}

    std::printf("-- Adder --\n");
    check("add 5,3", alu.execute(AluOp::ADD, 5, 3, flags).value, 8);
    check("add -5,3 (wrap)", alu.execute(AluOp::ADD, -5, 3, flags).value, static_cast<uint32_t>(-2));
    check("sub 10,4", alu.execute(AluOp::SUB, 10, 4, flags).value, 6);
    check("sub 3,10 (negative)", alu.execute(AluOp::SUB, 3, 10, flags).value, static_cast<uint32_t>(-7));

    std::printf("\n-- cmp: flags-only, value discarded --\n");
    {
        auto r = alu.execute(AluOp::CMP, 7, 7, flags);
        checkBool("cmp 7,7 -> E", r.flags.E, true);
        checkBool("cmp 7,7 -> GT", r.flags.GT, false);
    }
    {
        auto r = alu.execute(AluOp::CMP, 9, 4, flags);
        checkBool("cmp 9,4 -> E", r.flags.E, false);
        checkBool("cmp 9,4 -> GT", r.flags.GT, true);
    }
    {
        auto r = alu.execute(AluOp::CMP, 2, 9, flags);
        checkBool("cmp 2,9 -> GT", r.flags.GT, false);
    }

    std::printf("\n-- flag persistence: non-cmp ops must not touch flags --\n");
    {
        AluFlags afterCmp{true, false}; // pretend cmp just set E=true, GT=false
        auto r = alu.execute(AluOp::ADD, 1, 1, afterCmp);
        checkBool("add leaves E untouched", r.flags.E, true);
        checkBool("add leaves GT untouched", r.flags.GT, false);
    }

    std::printf("\n-- Multiplier --\n");
    check("mul 6,7", alu.execute(AluOp::MUL, 6, 7, flags).value, 42);
    check("mul -3,4", alu.execute(AluOp::MUL, -3, 4, flags).value, static_cast<uint32_t>(-12));

    std::printf("\n-- Divider --\n");
    check("div 17,5", alu.execute(AluOp::DIV, 17, 5, flags).value, 3);
    check("mod 17,5", alu.execute(AluOp::MOD, 17, 5, flags).value, 2);
    check("div -17,5 (trunc toward 0)", alu.execute(AluOp::DIV, -17, 5, flags).value, static_cast<uint32_t>(-3));
    {
        bool threw = false;
        try { alu.execute(AluOp::DIV, 5, 0, flags); }
        catch (const AluException&) { threw = true; }
        checkBool("div by zero throws AluException", threw, true);
    }
    {
        bool threw = false;
        try { alu.execute(AluOp::MOD, 5, 0, flags); }
        catch (const AluException&) { threw = true; }
        checkBool("mod by zero throws AluException", threw, true);
    }

    std::printf("\n-- Logical unit --\n");
    check("and 0xF0,0x1F", alu.execute(AluOp::AND, 0xF0, 0x1F, flags).value, 0x10);
    check("or  0xF0,0x0F", alu.execute(AluOp::OR, 0xF0, 0x0F, flags).value, 0xFF);
    check("not 0x00000000 (uses B, ignores A)", alu.execute(AluOp::NOT, 999, 0x00000000, flags).value, 0xFFFFFFFF);
    check("not 0xFFFFFFFF", alu.execute(AluOp::NOT, 0, static_cast<int32_t>(0xFFFFFFFF), flags).value, 0x00000000);

    std::printf("\n-- Shift unit (barrel shifter) --\n");
    check("lsl 0x1,4", alu.execute(AluOp::LSL, 0x1, 4, flags).value, 0x10);
    check("lsr 0x80000000,4 (zero-fill)", alu.execute(AluOp::LSR, static_cast<int32_t>(0x80000000), 4, flags).value, 0x08000000);
    check("asr 0x80000000,4 (sign-fill)", alu.execute(AluOp::ASR, static_cast<int32_t>(0x80000000), 4, flags).value, 0xF8000000);
    check("asr 0x00000010,2 (positive, no sign-fill)", alu.execute(AluOp::ASR, 0x10, 2, flags).value, 0x4);
    check("lsl shamt masked to 5 bits (shift by 36 == shift by 4)", alu.execute(AluOp::LSL, 0x1, 36, flags).value, 0x10);

    std::printf("\n-- Mov --\n");
    check("mov (uses B, ignores A)", alu.execute(AluOp::MOV, 999, 42, flags).value, 42);

    std::printf("\n-- mnemonicToAluOp --\n");
    {
        bool ok = true;
        try {
            ok = ok && (Alu::mnemonicToAluOp("add") == AluOp::ADD);
            ok = ok && (Alu::mnemonicToAluOp("cmp") == AluOp::CMP);
            ok = ok && (Alu::mnemonicToAluOp("asr") == AluOp::ASR);
        } catch (...) { ok = false; }
        checkBool("known mnemonics map correctly", ok, true);

        bool threw = false;
        try { Alu::mnemonicToAluOp("ld"); }
        catch (const std::invalid_argument&) { threw = true; }
        checkBool("'ld' rejected (handled via ADD, not this map)", threw, true);
    }

    std::printf("\n%d passed, %d failed\n", passCount, failCount);
    return failCount == 0 ? 0 : 1;
}
