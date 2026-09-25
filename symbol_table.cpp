#include "../include/symbol_table.hpp"
#include <stdexcept>

namespace risc201 {

bool SymbolTable::isDefined(const std::string& name) const {
    return table.find(name) != table.end();
}

void SymbolTable::define(const std::string& name, uint32_t address) {
    table[name] = address;
}

uint32_t SymbolTable::valueOf(const std::string& name) const {
    auto it = table.find(name);
    if (it == table.end()) throw std::runtime_error("undefined symbol: " + name);
    return it->second;
}

} // namespace risc201
