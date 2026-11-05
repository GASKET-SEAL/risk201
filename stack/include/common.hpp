// common.hpp -- shared constants and tiny string helpers
//
// ASSUMPTIONS (adjust to match your repo's ISA):
//   r13 = SP, word = 4 bytes, little-endian, registers r0..r15,
//   ld rd, off[rb] / st rs, off[rb], add|sub rd, rs, imm|rt, 12-bit signed imm.
#pragma once
#include <cstdint>
#include <string>

namespace risc201 {

constexpr int      NUM_REGS    = 16;
constexpr int      SP_REG      = 13;
constexpr uint32_t WORD        = 4;
constexpr int      MAX_IMM     = 2047;      // largest positive 12-bit signed immediate
constexpr uint32_t MEM_SIZE    = 0x10000;   // 64 KiB flat memory
constexpr uint32_t STACK_TOP   = MEM_SIZE;  // initial SP (full-descending)
constexpr uint32_t STACK_BASE  = 0xF000;    // lowest legal SP -> 4 KiB stack

std::string trim(const std::string& s);
std::string lower(std::string s);
std::string hex32(uint32_t v);

}  // namespace risc201
