#include <iostream>

#include "alu.hpp"
#include "execute_stage.hpp"

using namespace risc201;

int main() {

    Alu alu;

    RREXRegister input;

    input.valid = true;

    input.operand1 = 10;
    input.operand2 = 20;

    input.rd = 3;

    input.control.alu_control = ALUControl::ADD;
    input.control.register_write = true;
    input.control.write_back = WriteBackControl::ALU_RESULT;

    ExecuteStage ex(alu);

    AluFlags flags;

    EXMARegister output = ex.execute(input, flags);

    std::cout << "EX Stage Test\n";
    std::cout << "ALU Result = " << output.alu_result << '\n';
    std::cout << "Destination = R"
              << static_cast<int>(output.rd) << '\n';
    std::cout << "Valid = " << output.valid << '\n';

    return 0;
}
