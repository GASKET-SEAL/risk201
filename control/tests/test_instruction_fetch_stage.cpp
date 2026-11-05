#include <iostream>
#include <vector>

#include "instruction_fetch_stage.hpp"

using namespace risc201;

int main() {

    InstructionMemory memory;

    memory.load({
        0x12345678,
        0xABCDEF01
    });

    InstructionFetchStage fetch_stage(memory);

    IFIDRegister output = fetch_stage.fetch(0);

    std::cout << "IF Stage Test\n";
    std::cout << "Instruction = 0x"
              << std::hex << output.instruction << '\n';
    std::cout << "PC = "
              << std::dec << output.pc << '\n';
    std::cout << "PC + 4 = "
              << output.pc_plus_4 << '\n';
    std::cout << "Valid = "
              << output.valid << '\n';

    return 0;
}
