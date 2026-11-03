#ifndef DATA_MEMORY_HPP
#define DATA_MEMORY_HPP

#include <cstdint>
#include <vector>

namespace risc201 {

class DataMemory {
public:
    explicit DataMemory(std::size_t words = 1024);

    uint32_t read(uint32_t address) const;
    void write(uint32_t address, uint32_t value);
    void reset();

private:
    std::vector<uint32_t> memory;
};

}

#endif