#include "instruction_memory.hpp"

#include <stdexcept>

namespace risc201 {

InstructionMemory::InstructionMemory()
    : memory() {
}

uint32_t InstructionMemory::read(uint32_t address) const {

    if (address % 4 != 0) {
        throw std::out_of_range("Unaligned instruction address");
    }

    uint32_t index = address / 4;

    if (index >= memory.size()) {
        throw std::out_of_range("Instruction address out of range");
    }

    return memory[index];
}

void InstructionMemory::load(
    const std::vector<uint32_t>& instructions) {

    memory = instructions;
}

} // namespace risc201
