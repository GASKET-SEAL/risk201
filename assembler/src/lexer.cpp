#include "../include/lexer.hpp"
#include "../include/utilities.hpp"

namespace risc201 {

SourceLine Lexer::analyze(const std::string& rawLine, int lineNumber) const {
    SourceLine line;
    line.lineNumber = lineNumber;

    std::string s = util::trim(util::stripComment(rawLine));
    if (s.empty()) return line;

    std::string rest = s;
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        line.label = util::trim(s.substr(0, colon));
        line.hasLabel = !line.label.empty();
        rest = util::trim(s.substr(colon + 1));
    }

    line.empty = false;

    if (rest.empty()) return line;

    size_t sp = rest.find_first_of(" \t");
    line.mnemonic = util::toLower(sp == std::string::npos ? rest : rest.substr(0, sp));
    line.hasMnemonic = true;
    line.operandText = sp == std::string::npos ? "" : util::trim(rest.substr(sp + 1));

    return line;
}

} // namespace risc201
