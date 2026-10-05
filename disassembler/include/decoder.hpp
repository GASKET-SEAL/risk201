#pragma once
#include "opcode_table.hpp"
#include "types.hpp"

namespace risc201 {

class Decoder {
public:
    explicit Decoder(const OpcodeTable& opcodeTable);
    DecodedInstruction decode(uint32_t word, uint32_t address) const;

private:
    const OpcodeTable& opcodeTable;
};

}
