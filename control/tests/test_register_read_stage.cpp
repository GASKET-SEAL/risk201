#include <iostream>

#include "register_file.hpp"
#include "register_read_stage.hpp"

using namespace risc201;

int main() {

    RegisterFile registers;

    registers.write(1, 10);
    registers.write(2, 20);

    IDRRRegister input;

    input.valid = true;
    input.rs1 = 1;
    input.rs2 = 2;
    input.rd = 3;

    RegisterReadStage rr;

    RREXRegister output = rr.execute(input, registers);

    std::cout << "RR Stage Test\n";
    std::cout << "Operand 1 = " << output.operand1 << '\n';
    std::cout << "Operand 2 = " << output.operand2 << '\n';
    std::cout << "Destination = R" << static_cast<int>(output.rd) << '\n';
    std::cout << "Valid = " << output.valid << '\n';

    return 0;
}
