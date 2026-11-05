#include "control_memory.hpp"

#include <stdexcept>

namespace risc201 {

ControlMemory::ControlMemory()
    : memory(32) {

    // 0: ADD
    memory[0].alu_control = ALUControl::ADD;
    memory[0].register_write = true;
    memory[0].write_back = WriteBackControl::ALU_RESULT;

    // 1: SUB
    memory[1].alu_control = ALUControl::SUB;
    memory[1].register_write = true;
    memory[1].write_back = WriteBackControl::ALU_RESULT;

    // 2: MUL
    memory[2].alu_control = ALUControl::MUL;
    memory[2].register_write = true;
    memory[2].write_back = WriteBackControl::ALU_RESULT;

    // 3: DIV
    memory[3].alu_control = ALUControl::DIV;
    memory[3].register_write = true;
    memory[3].write_back = WriteBackControl::ALU_RESULT;

    // 4: MOD
    memory[4].alu_control = ALUControl::MOD;
    memory[4].register_write = true;
    memory[4].write_back = WriteBackControl::ALU_RESULT;

    // 5: CMP
    memory[5].alu_control = ALUControl::CMP;
    memory[5].flag_write = true;

    // 6: AND
    memory[6].alu_control = ALUControl::AND;
    memory[6].register_write = true;
    memory[6].write_back = WriteBackControl::ALU_RESULT;

    // 7: OR
    memory[7].alu_control = ALUControl::OR;
    memory[7].register_write = true;
    memory[7].write_back = WriteBackControl::ALU_RESULT;

    // 8: NOT
    memory[8].alu_control = ALUControl::NOT;
    memory[8].register_write = true;
    memory[8].write_back = WriteBackControl::ALU_RESULT;

    // 9: MOV
    memory[9].alu_control = ALUControl::MOV;
    memory[9].register_write = true;
    memory[9].write_back = WriteBackControl::ALU_RESULT;

    // 10: LSL
    memory[10].alu_control = ALUControl::LSL;
    memory[10].register_write = true;
    memory[10].write_back = WriteBackControl::ALU_RESULT;

    // 11: LSR
    memory[11].alu_control = ALUControl::LSR;
    memory[11].register_write = true;
    memory[11].write_back = WriteBackControl::ALU_RESULT;

    // 12: ASR
    memory[12].alu_control = ALUControl::ASR;
    memory[12].register_write = true;
    memory[12].write_back = WriteBackControl::ALU_RESULT;

    // 13: NOP
    // All control signals remain inactive.

    // 14: LD
    memory[14].alu_control = ALUControl::ADD;
    memory[14].use_immediate = true;
    memory[14].memory_read = true;
    memory[14].register_write = true;
    memory[14].write_back = WriteBackControl::MEMORY_DATA;

    // 15: ST
    memory[15].alu_control = ALUControl::ADD;
    memory[15].use_immediate = true;
    memory[15].memory_write = true;

    // 16: BEQ
    memory[16].branch_control = BranchControl::BEQ;
    memory[16].use_immediate = true;

    // 17: BGT
    memory[17].branch_control = BranchControl::BGT;
    memory[17].use_immediate = true;

    // 18: B
    memory[18].branch_control = BranchControl::B;
    memory[18].use_immediate = true;

    // 19: CALL
    memory[19].branch_control = BranchControl::CALL;
    memory[19].use_immediate = true;
    memory[19].register_write = true;
    memory[19].write_back = WriteBackControl::PC_PLUS_4;

    // 20: RET
    memory[20].branch_control = BranchControl::RET;

    // 21: HALT (stops the machine when it reaches write-back)
    memory[21].halt = true;

    // Opcodes 0..21 are implemented; 22..31 are illegal.
    for (size_t i = 0; i <= 21; ++i) {
        memory[i].defined = true;
    }
}

const MicroInstruction& ControlMemory::read(uint16_t address) const {
    if (address >= memory.size()) {
        throw std::out_of_range("Invalid control memory address");
    }

    return memory[address];
}

size_t ControlMemory::size() const {
    return memory.size();
}

} // namespace risc201