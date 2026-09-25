#include "../include/parser.hpp"
#include <fstream>
#include <stdexcept>

namespace risc201 {

std::vector<SourceLine> Parser::parseFile(const std::string& path) const {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open source file: " + path);

    std::vector<SourceLine> lines;
    std::string raw;
    int lineNumber = 0;
    while (std::getline(in, raw)) {
        lineNumber++;
        lines.push_back(lexer.analyze(raw, lineNumber));
    }
    return lines;
}

} // namespace risc201
