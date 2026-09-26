#include <iostream>
#include "../include/hardwired_control.hpp"

using namespace risc201;

void check(const char* name, bool got, bool expected) {
    std::cout << name << ": " << (got == expected ? "OK" : "FAIL")
              << " (got " << got << ", expected " << expected << ")\n";
}

int main() {
    // add = 00000 -> should write back, no st/ld/branch
    auto add = generateControlSignals(0b00000, false);
    check("add.isWb", add.isWb, true);
    check("add.isSt", add.isSt, false);

    // st = 01111 -> isSt true, isWb false
    auto st = generateControlSignals(0b01111, false);
    check("st.isSt", st.isSt, true);
    check("st.isWb", st.isWb, false);

    // ld = 01110 -> isLd true, isWb true (loads write a register)
    auto ld = generateControlSignals(0b01110, true);
    check("ld.isLd", ld.isLd, true);
    check("ld.isWb", ld.isWb, true);

    // beq = 10000 -> isBeq true, isWb false
    auto beq = generateControlSignals(0b10000, false);
    check("beq.isBeq", beq.isBeq, true);
    check("beq.isWb", beq.isWb, false);

    // call = 10011 -> isUBranch true, isCall true, isWb true (writes ra)
    auto call = generateControlSignals(0b10011, false);
    check("call.isUBranch", call.isUBranch, true);
    check("call.isCall", call.isCall, true);
    check("call.isWb", call.isWb, true);

    // ret = 10100 -> isRet true, isUBranch true, isWb false
    auto ret = generateControlSignals(0b10100, false);
    check("ret.isRet", ret.isRet, true);
    check("ret.isUBranch", ret.isUBranch, true);
    check("ret.isWb", ret.isWb, false);

    return 0;
}