#include "microprogram_sequencer.hpp"

#include <stdexcept>

namespace risc201 {

MicroProgramSequencer::MicroProgramSequencer(
    const ControlMemory& control_memory)
    : control_memory(control_memory),
      micro_pc_value(0) {
}

void MicroProgramSequencer::reset() {
    micro_pc_value = 0;
}

void MicroProgramSequencer::start(uint8_t opcode) {
    if (opcode >= control_memory.size()) {
        throw std::out_of_range("Invalid opcode for control memory");
    }

    micro_pc_value = opcode;
}

const MicroInstruction& MicroProgramSequencer::current() const {
    return control_memory.read(micro_pc_value);
}



uint16_t MicroProgramSequencer::micro_pc() const {
    return micro_pc_value;
}

} 