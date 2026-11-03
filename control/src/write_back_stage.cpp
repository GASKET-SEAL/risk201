#include "../include/write_back_stage.hpp"

namespace risc201 {

WriteBackStage::WriteBackStage(RegisterFile& registers)
    : registers(registers) {}

void WriteBackStage::execute(const MARWRegister& input) const {

    if (!input.valid)
        return;

    if (!input.control.register_write)
        return;

    uint32_t value = 0;

    switch (input.control.write_back) {

        case WriteBackControl::ALU_RESULT:
            value = input.alu_result;
            break;

        case WriteBackControl::MEMORY_DATA:
            value = input.memory_data;
            break;

        case WriteBackControl::PC_PLUS_4:
            value = input.pc_plus_4;
            break;

        case WriteBackControl::NONE:
        default:
            return;
    }

    registers.write(input.rd, value);
}

}