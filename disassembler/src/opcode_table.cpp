#include "../include/opcode_table.hpp"

namespace risc201 {

OpcodeTable::OpcodeTable() {
    table[0b00000] = {"add", Format::REG3};
    table[0b00001] = {"sub", Format::REG3};
    table[0b00010] = {"mul", Format::REG3};
    table[0b00011] = {"div", Format::REG3};
    table[0b00100] = {"mod", Format::REG3};
    table[0b00101] = {"cmp", Format::REG2_NO_RD};
    table[0b00110] = {"and", Format::REG3};
    table[0b00111] = {"or",  Format::REG3};
    table[0b01000] = {"not", Format::REG2_NO_RS1};
    table[0b01001] = {"mov", Format::REG2_NO_RS1};
    table[0b01010] = {"lsl", Format::REG3};
    table[0b01011] = {"lsr", Format::REG3};
    table[0b01100] = {"asr", Format::REG3};
    table[0b01101] = {"nop", Format::NO_OPERAND};
    table[0b01110] = {"ld",  Format::MEM};
    table[0b01111] = {"st",  Format::MEM};
    table[0b10000] = {"beq", Format::BRANCH};
    table[0b10001] = {"bgt", Format::BRANCH};
    table[0b10010] = {"b",   Format::BRANCH};
    table[0b10011] = {"call",Format::BRANCH};
    table[0b10100] = {"ret", Format::NO_OPERAND};
    table[0b10101] = {"halt", Format::NO_OPERAND};
    table[0b10110] = {"fadd", Format::REG3};
    table[0b10111] = {"fsub", Format::REG3};
    table[0b11000] = {"fmul", Format::REG3};
    table[0b11001] = {"fdiv", Format::REG3};
    table[0b11010] = {"fcmp", Format::REG2_NO_RD};
    table[0b11011] = {"itof", Format::REG2_NO_RS1};
    table[0b11100] = {"ftoi", Format::REG2_NO_RS1};
}

std::optional<OpcodeInfo> OpcodeTable::lookup(uint32_t opcode) const {
    auto it = table.find(opcode);
    if (it == table.end()) return std::nullopt;
    return it->second;
}

}
