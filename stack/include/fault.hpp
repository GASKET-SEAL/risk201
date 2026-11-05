// fault.hpp -- (f) runtime exception framework
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace risc201 {

enum class Fault {
    StackOverflow,        // SP / access went below the stack base
    StackUnderflow,       // SP / access went above the stack top (pop on empty)
    MisalignedSP,         // SP not a multiple of 4
    MisalignedAccess,     // ld/st address not word aligned
    BelowSP,              // access to already-freed stack memory (below SP)
    OutOfBounds,          // wild pointer outside memory
    CanaryCorrupted,      // linear overwrite reached the canary
    ShadowStackMismatch,  // saved return address differs from shadow copy
    ShadowStackEmpty,     // ret with no matching call
    UnbalancedFrame,      // SP != frame base at ret / SP inside own frame header
    BadInstruction,       // parse error / jump outside program / step limit
    MacroError            // push/pop macro validation failure (assembly time)
};

const char* faultName(Fault f);

struct StackException : std::runtime_error {
    Fault       fault;
    uint32_t    addr;
    int         srcLine;   // line in the ORIGINAL source (via the source-line map)
    uint32_t    pc;        // index of the faulting (expanded) instruction
    std::string srcText;   // text of the faulting instruction

    StackException(Fault f, const std::string& msg, uint32_t a = 0, int line = -1)
        : std::runtime_error(msg), fault(f), addr(a), srcLine(line), pc(0) {}
};

}  // namespace risc201
