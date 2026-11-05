#ifndef CONTROL_MEMORY_HPP
#define CONTROL_MEMORY_HPP

#include <cstdint>
#include <vector>
#include <cstddef>

#include "microinstruction.hpp"

namespace risc201 {

class ControlMemory {
public:
    ControlMemory();

    const MicroInstruction& read(uint16_t address) const;

    size_t size() const;

private:
    std::vector<MicroInstruction> memory;
};

} // namespace risc201

#endif