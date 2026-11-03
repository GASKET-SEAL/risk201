#ifndef REGISTER_FILE_HPP
#define REGISTER_FILE_HPP

#include <array>
#include <cstdint>

namespace risc201 {

class RegisterFile {
public:
    RegisterFile();

    uint32_t read(uint8_t index) const;

    void write(uint8_t index, uint32_t value);

    void reset();

private:
    std::array<uint32_t, 16> registers;
};

} 

#endif
