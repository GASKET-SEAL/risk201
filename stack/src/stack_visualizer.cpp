#include "stack_visualizer.hpp"

#include <iomanip>
#include <string>

#include "common.hpp"

namespace risc201 {

void renderStack(std::ostream& os, const StackGuard& g,
                 const std::function<uint32_t(uint32_t)>& rd,
                 uint32_t sp, size_t maxWords) {
    constexpr int W = 79;
    const uint32_t top = g.top(), base = g.base();
    const size_t used = (top - sp) / WORD;
    const auto& frames = g.frames();

    auto border = [&] { os << "  +" << std::string(W + 2, '-') << "+\n"; };
    auto banner = [&](const std::string& t) {
        os << "  | " << std::left << std::setw(W) << t.substr(0, W) << " |\n";
    };
    auto roleOf = [&](uint32_t addr, uint32_t val) {
        const StackGuard::Frame* own = nullptr;
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
            if (it->retSlot >= addr) { own = &*it; break; }
        std::string r;
        if (own && addr == own->retSlot) {
            r = val != own->retAddr
                ? "!! RET OVERWRITTEN f" + std::to_string(own->id) + " (was pc " +
                  std::to_string(own->retAddr) + ")"
                : "RET    f" + std::to_string(own->id) + " -> pc " + std::to_string(own->retAddr);
        } else if (own && g.canaryOn() && addr == own->canarySlot) {
            r = val != own->canary ? "!! CANARY SMASHED f" + std::to_string(own->id)
                                   : "CANARY f" + std::to_string(own->id);
        } else if (own) {
            r = "f" + std::to_string(own->id) + " locals / saved regs";
        } else {
            r = "data";
        }
        if (addr == sp) r += "  <== SP";
        return r.substr(0, 44);
    };
    auto row = [&](uint32_t addr) {
        uint32_t v = rd(addr);
        std::string off = addr == sp ? "SP" : "SP+" + std::to_string(addr - sp);
        os << "  | " << hex32(addr) << " | " << hex32(v) << " | " << std::setw(6) << std::left
           << off << " | " << std::setw(44) << std::left << roleOf(addr, v) << " |\n";
    };

    os << "\n  STACK MEMORY  (full-descending: push = SP-=4 then store; grows to lower addresses)\n";
    border();
    banner("== TOP of stack region " + hex32(top) + "  (empty-stack SP) ==");
    border();
    if (used == 0) banner("   (stack empty: SP == top)");
    const size_t head = used > maxWords ? 4 : used;
    for (size_t i = 0; i < head; ++i) row(top - WORD * (uint32_t)(i + 1));
    if (used > maxWords) {
        banner("   ... " + std::to_string(used - maxWords) + " more word(s) ...");
        for (size_t i = used - (maxWords - 4); i < used; ++i)
            row(top - WORD * (uint32_t)(i + 1));
    }
    border();
    banner("   free: " + std::to_string(sp - base) + " bytes below SP");
    banner("== BASE of stack region " + hex32(base) + "  (overflow limit) ==");
    border();

    const uint32_t size = top - base, usedB = top - sp;
    const int filled = (int)((uint64_t)usedB * 40 / size);
    os << "  SP=" << hex32(sp) << "  used " << usedB << "/" << size << " B  ["
       << std::string(filled, '#') << std::string(40 - filled, '.') << "]  frames="
       << frames.size() << "  shadow=" << (g.shadowOn() ? "on" : "off")
       << "  canary=" << (g.canaryOn() ? "on" : "off") << "\n";
    if (!frames.empty()) {
        os << "  shadow stack (top first):";
        for (auto it = frames.rbegin(); it != frames.rend(); ++it)
            os << "  f" << it->id << "->pc " << it->retAddr;
        os << "\n";
    }
}

}  // namespace risc201
