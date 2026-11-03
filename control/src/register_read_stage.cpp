#include "register_read_stage.hpp"

namespace risc201 {

RREXRegister RegisterReadStage::execute(
    const IDRRRegister& input,
    const RegisterFile& registers
) const {

    RREXRegister output;

    if (!input.valid) {
        return output;
    }

    output.valid = true;

    output.pc = input.pc;
    output.pc_plus_4 = input.pc_plus_4;

    output.operand1 = registers.read(input.rs1);
    output.operand2 = registers.read(input.rs2);
    output.store_data = registers.read(input.rd);

    output.immediate = input.immediate;

    output.rd = input.rd;
    output.rs1 = input.rs1;
    output.rs2 = input.rs2;

    output.control = input.control;

    return output;
}

} 
