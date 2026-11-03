#include "four_stage_components.hpp"

namespace risc201 {

FourStageComponents::FourStageComponents(
    const InstructionMemory& instruction_memory,
    const ControlMemory& control_memory,
    const Alu& alu,
    DataMemory& data_memory,
    RegisterFile& registers
)
    : fetch_stage(instruction_memory),
      decode_stage(control_memory),
      execute_stage(alu),
      memory_access_stage(data_memory),
      write_back_stage(registers),
      registers(registers) {}

IFIDRegister FourStageComponents::instructionFetch(uint32_t pc) const {
    return fetch_stage.fetch(pc);
}

RREXRegister FourStageComponents::operandFetch(const IFIDRegister& input) {
    IDRRRegister decoded = decode_stage.decode(input);
    return register_read_stage.execute(decoded, registers);
}

EXMARegister FourStageComponents::execute(
    const RREXRegister& input,
    AluFlags current_flags
) const {
    return execute_stage.execute(input, current_flags);
}

void FourStageComponents::memoryWriteback(const EXMARegister& input) {
    MARWRegister result = memory_access_stage.execute(input);
    write_back_stage.execute(result);
}

}
