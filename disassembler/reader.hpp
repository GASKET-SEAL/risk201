#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace risc201 {

class Reader {
public:
    static std::vector<uint32_t> readWords(const std::string& path);
};

}
