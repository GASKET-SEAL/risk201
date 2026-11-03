#include "control_packet.hpp"

namespace risc201 {

ControlPacket makeControlPacket(const MicroInstruction& instruction) {

    ControlPacket packet;

    packet.alu_control = instruction.alu_control;
    packet.branch_control = instruction.branch_control;
    packet.write_back = instruction.write_back;

    packet.use_immediate = instruction.use_immediate;

    packet.memory_read = instruction.memory_read;
    packet.memory_write = instruction.memory_write;

    packet.register_write = instruction.register_write;
    packet.flag_write = instruction.flag_write;

    return packet;
}

} // namespace risc201