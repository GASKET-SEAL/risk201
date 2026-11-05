#include "../include/data_memory.hpp"
#include <stdexcept>

namespace risc201 {

DataMemory::DataMemory(std::size_t words) : memory(words, 0) {}

uint32_t DataMemory::read(uint32_t address) const {
    if (address % 4 != 0)
        throw std::runtime_error("Unaligned data memory access");

    std::size_t index = address / 4;

    if (index >= memory.size())
        throw std::out_of_range("Data memory read out of range");

    return memory[index];
}

void DataMemory::write(uint32_t address, uint32_t value) {
    if (address % 4 != 0)
        throw std::runtime_error("Unaligned data memory access");

    std::size_t index = address / 4;

    if (index >= memory.size())
        throw std::out_of_range("Data memory write out of range");

    memory[index] = value;
}

void DataMemory::reset() {
    for (auto& word : memory)
        word = 0;
}

}
