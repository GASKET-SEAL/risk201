#pragma once
#include <string>
#include <vector>

namespace risc201 {

struct Diagnostic {
    int lineNumber;
    std::string message;
};

class Diagnostics {
public:
    void error(int lineNumber, const std::string& message);
    bool hasErrors() const;
    const std::vector<Diagnostic>& all() const;

private:
    std::vector<Diagnostic> items;
};

} // namespace risc201
