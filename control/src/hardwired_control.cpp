#include "../include/hardwired_control.hpp"

namespace risc201 {

ControlSignals generateControlSignals(uint8_t opcode, bool iBit) {
    bool op5 = (opcode >> 4) & 1;
    bool op4 = (opcode >> 3) & 1;
    bool op3 = (opcode >> 2) & 1;
    bool op2 = (opcode >> 1) & 1;
    bool op1 = opcode & 1;

    ControlSignals sig;

    // st = 01111
    sig.isSt = !op5 && op4 && op3 && op2 && op1;

    // ld = 01110
    sig.isLd = !op5 && op4 && op3 && op2 && !op1;

    // beq = 10000
    sig.isBeq = op5 && !op4 && !op3 && !op2 && !op1;

    // bgt = 10001
    sig.isBgt = op5 && !op4 && !op3 && !op2 && op1;

    // ret = 10100
    sig.isRet = op5 && !op4 && op3 && !op2 && !op1;

    // call = 10011
    sig.isCall = op5 && !op4 && !op3 && op2 && op1;

    // b = 10010
    sig.isUBranch = sig.isCall || sig.isRet ||
                    (op5 && !op4 && !op3 && op2 && !op1);  // b = 10010

    // I bit just passes straight through
    sig.isImmediate = iBit;

    // writeback happens for everything except: st, cmp, nop, beq, bgt, b, ret
    bool isCmp = !op5 && !op4 && op3 && !op2 && op1;  // cmp = 00101
    bool isNop = !op5 && op4 && op3 && op2 && op1;    // nop = 01101
    sig.isWb = !(sig.isSt || isCmp || isNop || sig.isBeq || sig.isBgt ||
                 sig.isUBranch);
    // isUBranch already covers b/call/ret, but call still needs a writeback (the return address)
    sig.isWb = sig.isWb || sig.isCall;

    return sig;
}

} 