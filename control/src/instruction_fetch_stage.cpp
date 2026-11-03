#include "instruction_fetch_stage.hpp"

namespace risc201 {

InstructionFetchStage::InstructionFetchStage(
    const InstructionMemory& memory)
    : memory(memory) {
}

IFIDRegister InstructionFetchStage::fetch(uint32_t pc) const {

    IFIDRegister output;

    output.instruction = memory.read(pc);
    output.pc = pc;
    output.pc_plus_4 = pc + 4;
    output.valid = true;

    return output;
}

} 