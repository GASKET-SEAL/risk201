#include "instruction_decode_stage.hpp"

namespace risc201 {

InstructionDecodeStage::InstructionDecodeStage(
    const ControlMemory& control_memory)
    : sequencer(control_memory) {
}

IDRRRegister InstructionDecodeStage::decode(
    const IFIDRegister& input) {

    IDRRRegister output;

    if (!input.valid) {
        return output;
    }

    uint32_t instruction = input.instruction;

    // Opcode occupies bits [31:27].
    uint8_t opcode =
        static_cast<uint8_t>((instruction >> 27) & 0x1F);

    // Register fields.
    output.rd =
        static_cast<uint8_t>((instruction >> 22) & 0x0F);

    output.rs1 =
        static_cast<uint8_t>((instruction >> 18) & 0x0F);

    output.rs2 =
        static_cast<uint8_t>((instruction >> 14) & 0x0F);

    // Default: 18-bit immediate.
    uint32_t imm18 = instruction & 0x3FFFF;

    if (imm18 & (1u << 17)) {
        output.immediate =
            static_cast<int32_t>(imm18 | 0xFFFC0000u);
    } else {
        output.immediate =
            static_cast<int32_t>(imm18);
    }

    // Branch instructions use a 27-bit signed offset.
    if (opcode >= 16 && opcode <= 19) {

        uint32_t offset = instruction & 0x07FFFFFF;

        if (offset & (1u << 26)) {
            output.immediate =
                static_cast<int32_t>(offset | 0xF8000000u);
        } else {
            output.immediate =
                static_cast<int32_t>(offset);
        }
    }

    // RET reads R15 according to the ISA convention.
    if (opcode == 20) {
        output.rs1 = 15;
    }

    // Obtain the microinstruction corresponding to the opcode.
    sequencer.start(opcode);

    const MicroInstruction& microinstruction =
        sequencer.current();

    output.control =
        makeControlPacket(microinstruction);

    output.valid = true;

    output.instruction = instruction;
    output.pc = input.pc;
    output.pc_plus_4 = input.pc_plus_4;

    return output;
}

} 
