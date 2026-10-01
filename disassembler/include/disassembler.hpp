#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace risc201 {

class Disassembler {
public:
    std::string disassemble(const std::string& inputPath);
    std::string disassembleWords(const std::vector<uint32_t>& words,
                                  const std::map<uint32_t, std::string>& symbols = {});
    std::string disassembleOne(uint32_t word, uint32_t address,
                                const std::map<uint32_t, std::string>& symbols = {});
};

}