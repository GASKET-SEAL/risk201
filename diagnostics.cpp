#include "../include/diagnostics.hpp"

namespace risc201 {

void Diagnostics::error(int lineNumber, const std::string& message) {
    items.push_back({lineNumber, message});
}

bool Diagnostics::hasErrors() const { return !items.empty(); }

const std::vector<Diagnostic>& Diagnostics::all() const { return items; }

} // namespace risc201
