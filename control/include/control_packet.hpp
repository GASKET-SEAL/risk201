#ifndef CONTROL_PACKET_HPP
#define CONTROL_PACKET_HPP

#include "microinstruction.hpp"

namespace risc201 {

struct ControlPacket {
    ALUControl alu_control = ALUControl::NONE;

    BranchControl branch_control = BranchControl::NONE;

    WriteBackControl write_back = WriteBackControl::NONE;

    bool use_immediate = false;

    bool memory_read = false;

    bool memory_write = false;

    bool register_write = false;

    bool flag_write = false;
};

ControlPacket makeControlPacket(const MicroInstruction& instruction);

} // namespace risc201

#endif