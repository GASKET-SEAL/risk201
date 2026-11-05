// stack_guard.hpp -- (f) bounds checks + shadow stack + canary
//
// Hook points for your CPU's execute stage:
//   checkSP(newSP)               before ANY write to SP commits
//   checkAccess(...)             before every ld / st
//   pushFrame(retSlot, retAddr)  on call  (returns the canary to store)
//   verifyReturn(sp, canary, ra) on ret   (throws StackException on tampering)
#pragma once
#include <cstdint>
#include <random>
#include <vector>

namespace risc201 {

class StackGuard {
public:
    struct Frame {
        uint32_t retSlot;     // address of saved return address
        uint32_t canarySlot;  // address of canary (== retSlot when canary is off)
        uint32_t retAddr;     // shadow copy of the return address
        uint32_t canary;      // expected canary value
        int      id;
    };

    StackGuard(uint32_t base, uint32_t top, bool shadow = true, bool canary = true);

    uint32_t base() const { return base_; }
    uint32_t top()  const { return top_; }
    bool shadowOn() const { return shadow_; }
    bool canaryOn() const { return canary_; }
    const std::vector<Frame>& frames() const { return frames_; }

    // Region / SP bounds check (overflow, underflow, alignment, frame header).
    void checkSP(int64_t sp) const;

    // Access check: misaligned, below-SP, outside region, wild pointer.
    void checkAccess(int64_t addr, bool store, uint32_t sp, bool viaSP) const;

    // call: register a frame; returns the canary value to place in memory.
    uint32_t pushFrame(uint32_t retSlot, uint32_t retAddr);

    // ret: verify frame, canary and shadow copy; pops the frame on success.
    void verifyReturn(uint32_t sp, uint32_t canaryInMem, uint32_t retInMem);

private:
    uint32_t           base_, top_;
    bool               shadow_, canary_;
    std::vector<Frame> frames_;
    int                nextId_ = 1;
    std::mt19937       rng_;
};

}  // namespace risc201
