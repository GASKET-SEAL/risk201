#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace risc201 {

class SymbolTable {
public:
    bool isDefined(const std::string& name) const;
    void define(const std::string& name, uint32_t address);
    uint32_t valueOf(const std::string& name) const;

private:
    std::map<std::string, uint32_t> table;
};

} // namespace risc201
