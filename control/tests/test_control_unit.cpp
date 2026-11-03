
#include <cassert>
#include <iostream>
#include <stdexcept>

#include "control_memory.hpp"
#include "microprogram_sequencer.hpp"
#include "control_packet.hpp"

using namespace risc201;

void testOpcode(uint8_t opcode, const char* name) {
    ControlMemory control_memory;
    MicroProgramSequencer sequencer(control_memory);

    sequencer.start(opcode);

    const MicroInstruction& instruction =
        sequencer.current();

    // Direct lookup keeps the selected address equal to the opcode.
    assert(sequencer.micro_pc() == opcode);

    ControlPacket packet = makeControlPacket(instruction);

    std::cout << name
              << " | opcode = " << static_cast<int>(opcode)
              << " | microPC = " << sequencer.micro_pc()
              << " | ALU = " << static_cast<int>(packet.alu_control)
              << " | branch = "
              << static_cast<int>(packet.branch_control)
              << " | regWrite = " << packet.register_write
              << " | memRead = " << packet.memory_read
              << " | memWrite = " << packet.memory_write
              << " | flagWrite = " << packet.flag_write
              << '\n';
}

int main() {
    testOpcode(0, "ADD");
    testOpcode(14, "LD");
    testOpcode(16, "BEQ");

    ControlMemory memory;
    MicroProgramSequencer sequencer(memory);

    sequencer.start(0);
    assert(sequencer.current().alu_control == ALUControl::ADD);
    assert(sequencer.current().register_write);

    sequencer.start(14);
    assert(sequencer.current().memory_read);
    assert(sequencer.current().write_back ==
           WriteBackControl::MEMORY_DATA);

    sequencer.start(16);
    assert(sequencer.current().branch_control ==
           BranchControl::BEQ);

    bool threw = false;

    try {
        sequencer.start(32);
    } catch (const std::out_of_range&) {
        threw = true;
    }

    assert(threw);

    std::cout << "Control lookup tests passed\n";
    return 0;
}
