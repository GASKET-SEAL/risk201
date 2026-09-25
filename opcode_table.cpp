#include "../include/opcode_table.hpp"
#include <stdexcept>

namespace risc201 {

OpcodeTable::OpcodeTable() {
    table["add"]  = {0b00000, Format::REG3};
    table["sub"]  = {0b00001, Format::REG3};
    table["mul"]  = {0b00010, Format::REG3};
    table["div"]  = {0b00011, Format::REG3};
    table["mod"]  = {0b00100, Format::REG3};
    table["cmp"]  = {0b00101, Format::REG2_NO_RD};
    table["and"]  = {0b00110, Format::REG3};
    table["or"]   = {0b00111, Format::REG3};
    table["not"]  = {0b01000, Format::REG2_NO_RS1};
    table["mov"]  = {0b01001, Format::REG2_NO_RS1};
    table["lsl"]  = {0b01010, Format::REG3};
    table["lsr"]  = {0b01011, Format::REG3};
    table["asr"]  = {0b01100, Format::REG3};
    table["nop"]  = {0b01101, Format::NO_OPERAND};
    table["ld"]   = {0b01110, Format::MEM};
    table["st"]   = {0b01111, Format::MEM};
    table["beq"]  = {0b10000, Format::BRANCH};
    table["bgt"]  = {0b10001, Format::BRANCH};
    table["b"]    = {0b10010, Format::BRANCH};
    table["call"] = {0b10011, Format::BRANCH};
    table["ret"]  = {0b10100, Format::NO_OPERAND};
}

bool OpcodeTable::exists(const std::string& mnemonic) const {
    return table.find(mnemonic) != table.end();
}

const OpcodeInfo& OpcodeTable::get(const std::string& mnemonic) const {
    auto it = table.find(mnemonic);
    if (it == table.end()) throw std::runtime_error("unknown mnemonic: " + mnemonic);
    return it->second;
}

} // namespace risc201
