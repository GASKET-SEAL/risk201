#ifndef INSTRUCTION_FETCH_STAGE_HPP
#define INSTRUCTION_FETCH_STAGE_HPP

#include <cstdint>

#include "instruction_memory.hpp"
#include "pipeline_registers.hpp"

namespace risc201 {

class InstructionFetchStage {
public:
    explicit InstructionFetchStage(const InstructionMemory& memory);

    IFIDRegister fetch(uint32_t pc) const;

private:
    const InstructionMemory& memory;
};

} // namespace risc201

#endif
