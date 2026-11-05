#include "fault.hpp"

namespace risc201 {

const char* faultName(Fault f) {
    switch (f) {
        case Fault::StackOverflow:       return "STACK_OVERFLOW";
        case Fault::StackUnderflow:      return "STACK_UNDERFLOW";
        case Fault::MisalignedSP:        return "MISALIGNED_SP";
        case Fault::MisalignedAccess:    return "MISALIGNED_ACCESS";
        case Fault::BelowSP:             return "ACCESS_BELOW_SP";
        case Fault::OutOfBounds:         return "OUT_OF_BOUNDS";
        case Fault::CanaryCorrupted:     return "CANARY_CORRUPTED";
        case Fault::ShadowStackMismatch: return "SHADOW_STACK_MISMATCH";
        case Fault::ShadowStackEmpty:    return "SHADOW_STACK_EMPTY";
        case Fault::UnbalancedFrame:     return "UNBALANCED_FRAME";
        case Fault::BadInstruction:      return "BAD_INSTRUCTION";
        case Fault::MacroError:          return "MACRO_ERROR";
    }
    return "UNKNOWN";
}

}  // namespace risc201
