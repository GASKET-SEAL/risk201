#include "../include/memory_access_stage.hpp"

namespace risc201 {

MemoryAccessStage::MemoryAccessStage(DataMemory& memory)
    : memory(memory) {}

MARWRegister MemoryAccessStage::execute(
    const EXMARegister& input
) const {

    MARWRegister output;

    if (!input.valid)
        return output;

    output.valid = true;
    output.pc = input.pc;
    output.pc_plus_4 = input.pc_plus_4;
    output.alu_result = input.alu_result;
    output.rd = input.rd;
    output.control = input.control;

    if (input.control.memory_read) {
        output.memory_data = memory.read(input.alu_result);
    }

    if (input.control.memory_write) {
        memory.write(input.alu_result, input.store_data);
    }

    return output;
}

}

