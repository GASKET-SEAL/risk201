#pragma once
#include "types.hpp"
#include <map>
#include <optional>

namespace risc201 {

class OpcodeTable {
public:
    OpcodeTable();
    std::optional<OpcodeInfo> lookup(uint32_t opcode) const;

private:
    std::map<uint32_t, OpcodeInfo> table;
};

}
