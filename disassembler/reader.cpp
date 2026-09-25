#include "../include/reader.hpp"
#include <fstream>
#include <stdexcept>

namespace risc201 {

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::vector<uint32_t> readHex(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint32_t> words;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        words.push_back((uint32_t)std::stoul(line, nullptr, 16));
    }
    return words;
}

static std::vector<uint32_t> readBin(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint32_t> words;
    unsigned char bytes[4];
    while (in.read((char*)bytes, 4)) {
        uint32_t w = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
                     (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
        words.push_back(w);
    }
    return words;
}

std::vector<uint32_t> Reader::readWords(const std::string& path) {
    if (endsWith(path, ".bin")) return readBin(path);
    return readHex(path);
}

}
