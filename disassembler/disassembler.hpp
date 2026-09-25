#pragma once
#include <string>

namespace risc201 {

class Disassembler {
public:
    std::string disassemble(const std::string& inputPath);
};

}
