#ifndef FOUR_STAGE_COMPONENTS_HPP
#define FOUR_STAGE_COMPONENTS_HPP

#include "instruction_fetch_stage.hpp"
#include "instruction_decode_stage.hpp"
#include "register_read_stage.hpp"
#include "execute_stage.hpp"
#include "memory_access_stage.hpp"
#include "write_back_stage.hpp"

namespace risc201 {

class FourStageComponents {
public:
    FourStageComponents(
        const InstructionMemory& instruction_memory,
        const ControlMemory& control_memory,
        const Alu& alu,
        DataMemory& data_memory,
        RegisterFile& registers
    );

    IFIDRegister instructionFetch(uint32_t pc) const;

    RREXRegister operandFetch(const IFIDRegister& input);

    EXMARegister execute(
        const RREXRegister& input,
        AluFlags current_flags
    ) const;

    void memoryWriteback(const EXMARegister& input);

private:
    InstructionFetchStage fetch_stage;
    InstructionDecodeStage decode_stage;
    RegisterReadStage register_read_stage;
    ExecuteStage execute_stage;
    MemoryAccessStage memory_access_stage;
    WriteBackStage write_back_stage;
    RegisterFile& registers;
};

}

#endif
