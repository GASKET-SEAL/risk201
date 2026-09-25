#pragma once
#include "lexer.hpp"
#include "types.hpp"
#include <string>
#include <vector>

namespace risc201 {

class Parser {
public:
    std::vector<SourceLine> parseFile(const std::string& path) const;

private:
    Lexer lexer;
};

} // namespace risc201
