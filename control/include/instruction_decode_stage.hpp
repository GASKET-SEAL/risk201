#ifndef INSTRUCTION_DECODE_STAGE_HPP
#define INSTRUCTION_DECODE_STAGE_HPP

#include "control_memory.hpp"
#include "microprogram_sequencer.hpp"
#include "pipeline_registers.hpp"

namespace risc201 {

class InstructionDecodeStage {
public:
    explicit InstructionDecodeStage(const ControlMemory& control_memory);

    IDRRRegister decode(const IFIDRegister& input);

private:
    MicroProgramSequencer sequencer;
};

} 
#endif
