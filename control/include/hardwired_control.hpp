#pragma once
#include <cstdint>

namespace risc201 {

struct ControlSignals {
    bool isSt = false;
    bool isLd = false;
    bool isBeq = false;
    bool isBgt = false;
    bool isRet = false;
    bool isImmediate = false;
    bool isWb = false;
    bool isUBranch = false;
    bool isCall = false;
};

ControlSignals generateControlSignals(uint8_t opcode, bool iBit);

} 