#include "stack_guard.hpp"

#include <string>

#include "common.hpp"
#include "fault.hpp"

namespace risc201 {

StackGuard::StackGuard(uint32_t base, uint32_t top, bool shadow, bool canary)
    : base_(base), top_(top), shadow_(shadow), canary_(canary),
      rng_(std::random_device{}()) {}

void StackGuard::checkSP(int64_t sp) const {
    if (sp & 3)
        throw StackException(Fault::MisalignedSP,
            "SP " + hex32((uint32_t)sp) + " is not 4-byte aligned", (uint32_t)sp);
    if (sp < (int64_t)base_)
        throw StackException(Fault::StackOverflow,
            "SP would become " + hex32((uint32_t)sp) + ", below stack base " +
            hex32(base_), (uint32_t)sp);
    if (sp > (int64_t)top_)
        throw StackException(Fault::StackUnderflow,
            "SP would become " + hex32((uint32_t)sp) + ", above stack top " +
            hex32(top_), (uint32_t)sp);
    // inside a function SP may never rise into its own saved header (canary/ret)
    if (!frames_.empty() && sp > (int64_t)frames_.back().canarySlot)
        throw StackException(Fault::UnbalancedFrame,
            "SP would become " + hex32((uint32_t)sp) + ", inside the saved frame header of f" +
            std::to_string(frames_.back().id) + " (header starts at " +
            hex32(frames_.back().canarySlot) + ")", (uint32_t)sp);
}

void StackGuard::checkAccess(int64_t addr, bool store, uint32_t sp, bool viaSP) const {
    const std::string op = store ? "store" : "load";
    if (addr & 3)
        throw StackException(Fault::MisalignedAccess,
            op + " at " + hex32((uint32_t)addr) + " is not word aligned", (uint32_t)addr);
    const bool inRegion = addr >= (int64_t)base_ && addr + 4 <= (int64_t)top_;
    if (inRegion) {
        if (addr < (int64_t)sp)
            throw StackException(Fault::BelowSP,
                op + " at " + hex32((uint32_t)addr) + " is below SP " + hex32(sp) +
                " (freed stack memory)", (uint32_t)addr);
        return;
    }
    if (viaSP) {
        if (addr >= (int64_t)top_)
            throw StackException(Fault::StackUnderflow,
                op + " at " + hex32((uint32_t)addr) + " is above the stack top " +
                hex32(top_) + " (pop/read on empty stack)", (uint32_t)addr);
        throw StackException(Fault::StackOverflow,
            op + " at " + hex32((uint32_t)addr) + " is below the stack base " +
            hex32(base_), (uint32_t)addr);
    }
    if (addr < 0 || addr + 4 > (int64_t)top_)
        throw StackException(Fault::OutOfBounds,
            op + " at " + hex32((uint32_t)addr) + " is outside memory", (uint32_t)addr);
}

uint32_t StackGuard::pushFrame(uint32_t retSlot, uint32_t retAddr) {
    uint32_t c = 0;
    if (canary_) {
        do { c = rng_() & 0xFFFFFF00u; } while (c == 0);  // low byte 0x00 = terminator
    }
    frames_.push_back({retSlot, canary_ ? retSlot - WORD : retSlot, retAddr, c, nextId_++});
    return c;
}

void StackGuard::verifyReturn(uint32_t sp, uint32_t canaryInMem, uint32_t retInMem) {
    if (frames_.empty())
        throw StackException(Fault::ShadowStackEmpty, "ret without a matching call");
    const Frame& f = frames_.back();
    if (sp != f.canarySlot)
        throw StackException(Fault::UnbalancedFrame,
            "SP=" + hex32(sp) + " at ret, expected " + hex32(f.canarySlot) +
            " (locals not released or over-popped)", sp);
    if (canary_ && canaryInMem != f.canary)
        throw StackException(Fault::CanaryCorrupted,
            "canary of f" + std::to_string(f.id) + " at " + hex32(f.canarySlot) +
            " is " + hex32(canaryInMem) + ", expected " + hex32(f.canary), f.canarySlot);
    if (shadow_ && retInMem != f.retAddr)
        throw StackException(Fault::ShadowStackMismatch,
            "return address of f" + std::to_string(f.id) + " at " + hex32(f.retSlot) +
            " is " + hex32(retInMem) + ", shadow stack says " + hex32(f.retAddr),
            f.retSlot);
    frames_.pop_back();
}

}  // namespace risc201
