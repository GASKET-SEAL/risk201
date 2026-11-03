#include <iostream>

#include "instruction_decode_stage.hpp"

using namespace risc201;

int main() {

    /*
        ADD R3, R1, R2

        opcode = 0
        rd     = 3
        rs1    = 1
        rs2    = 2
    */

    uint32_t instruction = 0;

    instruction |= (0u << 27);   // opcode
    instruction |= (3u << 22);   // rd
    instruction |= (1u << 18);   // rs1
    instruction |= (2u << 14);   // rs2

    IFIDRegister input;

    input.valid = true;
    input.instruction = instruction;
    input.pc = 0;
    input.pc_plus_4 = 4;

    ControlMemory control_memory;

    InstructionDecodeStage id(control_memory);

    IDRRRegister output = id.decode(input);

    std::cout << "ID Stage Test\n";

    std::cout << "Opcode = "
              << static_cast<int>(
                     (instruction >> 27) & 0x1F)
              << '\n';

    std::cout << "rd = R"
              << static_cast<int>(output.rd) << '\n';

    std::cout << "rs1 = R"
              << static_cast<int>(output.rs1) << '\n';

    std::cout << "rs2 = R"
              << static_cast<int>(output.rs2) << '\n';

    std::cout << "Immediate = "
              << output.immediate << '\n';

    std::cout << "ALU control = "
              << static_cast<int>(
                     output.control.alu_control)
              << '\n';

    std::cout << "Register write = "
              << output.control.register_write << '\n';

    std::cout << "Valid = "
              << output.valid << '\n';

    return 0;
}
