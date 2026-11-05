// stack_visualizer.hpp -- (g) active stack memory visualiser (ASCII frame renderer)
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>

#include "stack_guard.hpp"

namespace risc201 {

// rd(addr) must return the 32-bit word stored at addr.
void renderStack(std::ostream& os, const StackGuard& g,
                 const std::function<uint32_t(uint32_t)>& rd,
                 uint32_t sp, size_t maxWords = 24);

}  // namespace risc201
