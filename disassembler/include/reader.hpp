#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace risc201 {

class Reader {
public:
    static std::vector<uint32_t> readWords(const std::string& path);
    static std::map<uint32_t, std::string> readSymbols(const std::string& path);
};

}