#include <cassert>
#include <iostream>
#include <vector>

#include "four_stage_components.hpp"

using namespace risc201;

int main() {
    InstructionMemory instruction_memory;
    ControlMemory control_memory;
    Alu alu;
    DataMemory data_memory;
    RegisterFile registers;

    registers.write(1, 10);
    registers.write(2, 20);

    uint32_t instruction = 0;
    instruction |= (0u << 27);
    instruction |= (3u << 22);
    instruction |= (1u << 18);
    instruction |= (2u << 14);

    instruction_memory.load({instruction});

    FourStageComponents pipeline(
        instruction_memory,
        control_memory,
        alu,
        data_memory,
        registers
    );

    IFIDRegister if_output = pipeline.instructionFetch(0);
    assert(if_output.valid);

    RREXRegister of_output = pipeline.operandFetch(if_output);
    assert(of_output.valid);
    assert(of_output.operand1 == 10);
    assert(of_output.operand2 == 20);

    AluFlags flags;
    EXMARegister ex_output = pipeline.execute(of_output, flags);
    assert(ex_output.valid);
    assert(ex_output.alu_result == 30);

    pipeline.memoryWriteback(ex_output);
    assert(registers.read(3) == 30);

    std::cout << "Four-stage ADD test passed!\n";
    std::cout << "R3 = " << registers.read(3) << '\n';

    return 0;
}
