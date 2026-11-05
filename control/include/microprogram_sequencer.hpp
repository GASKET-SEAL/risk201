#ifndef MICROPROGRAM_SEQUENCER_HPP
#define MICROPROGRAM_SEQUENCER_HPP

#include <cstdint>

#include "control_memory.hpp"

namespace risc201 {

class MicroProgramSequencer {
public:
    explicit MicroProgramSequencer(const ControlMemory& control_memory);

    void reset();

    void start(uint8_t opcode);
    
    const MicroInstruction& current() const;

    uint16_t micro_pc() const;

private:
    const ControlMemory& control_memory;
    uint16_t micro_pc_value;
};

} 

#endif