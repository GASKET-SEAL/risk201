#pragma once
#include <string>
#include <vector>

namespace risc201::util {

std::string trim(const std::string& s);
std::string stripComment(const std::string& s);
std::string toLower(const std::string& s);
std::vector<std::string> splitComma(const std::string& s);
bool isRegisterToken(const std::string& tok, int& regOut);
bool parseImmediate(const std::string& tok, long& out);
bool fitsSigned(long v, int bits);
bool parseMemOperand(const std::string& tok, long& immOut, int& regOut);

} // namespace risc201::util
