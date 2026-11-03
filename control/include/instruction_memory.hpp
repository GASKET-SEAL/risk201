#ifndef INSTRUCTION_MEMORY_HPP
#define INSTRUCTION_MEMORY_HPP

#include <cstdint>
#include <vector>

namespace risc201 {

class InstructionMemory {
public:
    InstructionMemory();

    uint32_t read(uint32_t address) const;

    void load(const std::vector<uint32_t>& instructions);

private:
    std::vector<uint32_t> memory;
};

} 

#endif