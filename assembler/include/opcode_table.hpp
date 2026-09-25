#pragma once
#include "types.hpp"
#include <map>
#include <string>

namespace risc201 {

class OpcodeTable {
public:
    OpcodeTable();
    bool exists(const std::string& mnemonic) const;
    const OpcodeInfo& get(const std::string& mnemonic) const;

private:
    std::map<std::string, OpcodeInfo> table;
};

} // namespace risc201
