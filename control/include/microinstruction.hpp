#ifndef MICROINSTRUCTION_HPP
#define MICROINSTRUCTION_HPP

#include <cstdint>

namespace risc201 {

enum class ALUControl : uint8_t {
    NONE,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    AND,
    OR,
    NOT,
    MOV,
    LSL,
    LSR,
    ASR,
    CMP,
    FADD,
    FSUB,
    FMUL,
    FDIV,
    FCMP,
    ITOF,
    FTOI
};

enum class BranchControl : uint8_t {
    NONE,
    BEQ,
    BGT,
    B,
    CALL,
    RET
};

enum class WriteBackControl : uint8_t {
    NONE,
    ALU_RESULT,
    MEMORY_DATA,
    PC_PLUS_4
};

enum class MicroNextControl : uint8_t {
    SEQUENTIAL,
    JUMP,
    CONDITIONAL
};

struct MicroInstruction {

    // PC / instruction control
    bool pc_write = false;
    bool instruction_read = false;

    // Data-memory control
    bool memory_read = false;
    bool memory_write = false;

    // Register-file control
    bool register_write = false;

    // Immediate generation
    bool use_immediate = false;

    // Datapath control
    ALUControl alu_control = ALUControl::NONE;
    BranchControl branch_control = BranchControl::NONE;
    WriteBackControl write_back = WriteBackControl::NONE;

    // Architectural flags
    bool flag_write = false;

    // Marks an implemented opcode (unused opcodes raise an illegal-opcode fault)
    bool defined = false;

    // Stops the machine when this instruction retires (HALT)
    bool halt = false;

    // Pipeline control
    bool stall = false;
    bool flush = false;

    // Microprogram sequencing
    uint16_t next_address = 0;
    MicroNextControl next_control = MicroNextControl::SEQUENTIAL;
};

} // namespace risc201

#endif