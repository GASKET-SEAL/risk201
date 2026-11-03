#include "register_file.hpp"

namespace risc201 {

RegisterFile::RegisterFile() {
    reset();
}

uint32_t RegisterFile::read(uint8_t index) const {
    return registers.at(index);
}

void RegisterFile::write(uint8_t index, uint32_t value) {
    registers.at(index) = value;
}

void RegisterFile::reset() {
    registers.fill(0);
}

} // namespace risc201
