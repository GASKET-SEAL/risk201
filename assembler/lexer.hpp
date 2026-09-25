#pragma once
#include "types.hpp"
#include <string>

namespace risc201 {

class Lexer {
public:
    SourceLine analyze(const std::string& rawLine, int lineNumber) const;
};

} // namespace risc201
