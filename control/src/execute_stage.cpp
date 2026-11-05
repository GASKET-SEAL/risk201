#include "execute_stage.hpp"

namespace risc201 {

ExecuteStage::ExecuteStage(const Alu& alu)
    : alu(alu) {
}

EXMARegister ExecuteStage::execute(
    const RREXRegister& input,
    AluFlags current_flags
) const {

    EXMARegister output;

    if (!input.valid) {
        return output;
    }

    output.valid = true;

    output.pc = input.pc;
    output.pc_plus_4 = input.pc_plus_4;
    output.rd = input.rd;

    output.control = input.control;

    AluOp alu_op;

    switch (input.control.alu_control) {

        case ALUControl::ADD:
            alu_op = AluOp::ADD;
            break;

        case ALUControl::SUB:
            alu_op = AluOp::SUB;
            break;

        case ALUControl::MUL:
            alu_op = AluOp::MUL;
            break;

        case ALUControl::DIV:
            alu_op = AluOp::DIV;
            break;

        case ALUControl::MOD:
            alu_op = AluOp::MOD;
            break;

        case ALUControl::AND:
            alu_op = AluOp::AND;
            break;

        case ALUControl::OR:
            alu_op = AluOp::OR;
            break;

        case ALUControl::NOT:
            alu_op = AluOp::NOT;
            break;

        case ALUControl::MOV:
            alu_op = AluOp::MOV;
            break;

        case ALUControl::LSL:
            alu_op = AluOp::LSL;
            break;

        case ALUControl::LSR:
            alu_op = AluOp::LSR;
            break;

        case ALUControl::ASR:
            alu_op = AluOp::ASR;
            break;

        case ALUControl::CMP:
            alu_op = AluOp::CMP;
            break;

        case ALUControl::NONE:
        default:
            return output;
    }

    int32_t operand2 = input.control.use_immediate
        ? input.immediate
        : static_cast<int32_t>(input.operand2);

    AluResult result = alu.execute(
        alu_op,
        static_cast<int32_t>(input.operand1),
        operand2,
        current_flags
    );

    output.alu_result = result.value;
    output.flags = result.flags;
    output.nzcv = result.nzcv;

    output.store_data = input.store_data;

    return output;
}

} 
