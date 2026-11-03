#ifndef PIPELINE_REGISTERS_HPP
#define PIPELINE_REGISTERS_HPP

#include <cstdint>

#include "control_packet.hpp"
#include "alu.hpp"

namespace risc201 {

struct IFIDRegister {
    bool valid = false;

    uint32_t instruction = 0;

    uint32_t pc = 0;
    uint32_t pc_plus_4 = 0;
};

struct IDRRRegister {
    bool valid = false;

    uint32_t instruction = 0;

    uint32_t pc = 0;
    uint32_t pc_plus_4 = 0;

    uint8_t rs1 = 0;
    uint8_t rs2 = 0;
    uint8_t rd = 0;

    int32_t immediate = 0;

    ControlPacket control;
};

struct RREXRegister {
    bool valid = false;

    uint32_t pc = 0;
    uint32_t pc_plus_4 = 0;

    uint32_t operand1 = 0;
    uint32_t operand2 = 0;
     uint32_t store_data = 0;

    int32_t immediate = 0;

    uint8_t rd = 0;
    uint8_t rs1 = 0;
    uint8_t rs2 = 0;
   

    ControlPacket control;
};

struct EXMARegister {
    bool valid = false;

    uint32_t pc = 0;
    uint32_t pc_plus_4 = 0;

    uint32_t alu_result = 0;

    AluFlags flags;
    AluNzcv nzcv;

    uint32_t store_data = 0;

    uint8_t rd = 0;

    ControlPacket control;
};
struct MARWRegister {
    bool valid = false;

    uint32_t pc = 0;
    uint32_t pc_plus_4 = 0;

    uint32_t alu_result = 0;
    uint32_t memory_data = 0;

    uint8_t rd = 0;

    ControlPacket control;
};

} 
#endif
