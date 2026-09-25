#include "../include/utilities.hpp"
#include <cctype>
#include <sstream>

namespace risc201::util {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string stripComment(const std::string& s) {
    size_t p = s.find_first_of(";#");
    return p == std::string::npos ? s : s.substr(0, p);
}

std::string toLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(trim(item));
    return out;
}

bool isRegisterToken(const std::string& tok, int& regOut) {
    if (tok.size() < 2 || (tok[0] != 'r' && tok[0] != 'R')) return false;
    for (size_t i = 1; i < tok.size(); i++)
        if (!isdigit((unsigned char)tok[i])) return false;
    int v = std::stoi(tok.substr(1));
    if (v < 0 || v > 15) return false;
    regOut = v;
    return true;
}

bool parseImmediate(const std::string& tok, long& out) {
    try {
        size_t idx;
        long v = std::stol(tok, &idx, 0);
        if (idx != tok.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool fitsSigned(long v, int bits) {
    long lo = -(1L << (bits - 1));
    long hi = (1L << (bits - 1)) - 1;
    return v >= lo && v <= hi;
}

bool parseMemOperand(const std::string& tok, long& immOut, int& regOut) {
    size_t open = tok.find('[');
    size_t close = tok.find(']');
    if (open == std::string::npos || close == std::string::npos || close < open) return false;
    std::string immPart = trim(tok.substr(0, open));
    std::string regPart = trim(tok.substr(open + 1, close - open - 1));
    immOut = 0;
    if (!immPart.empty() && !parseImmediate(immPart, immOut)) return false;
    return isRegisterToken(regPart, regOut);
}

} // namespace risc201::util
