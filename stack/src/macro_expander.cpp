#include "macro_expander.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "common.hpp"
#include "fault.hpp"

namespace risc201 {

std::vector<AsmLine> MacroExpander::expand(const std::string& source) {
    out_.clear();
    pend_.clear();
    stats_ = {};
    kind_  = Kind::None;
    std::istringstream in(source);
    std::string raw;
    int ln = 0;
    while (std::getline(in, raw)) handleLine(raw, ++ln);
    flush();
    return out_;
}

void MacroExpander::fail(int ln, const std::string& msg) {
    throw StackException(Fault::MacroError,
                         "line " + std::to_string(ln) + ": " + msg, 0, ln);
}

std::string MacroExpander::R(int r) { return "r" + std::to_string(r); }

void MacroExpander::emit(const std::string& t, int line) { out_.push_back({t, line}); }

std::string MacroExpander::stripComment(const std::string& s) {
    size_t cut = s.size();
    for (const char* m : {";", "#", "//"}) {
        size_t p = s.find(m);
        if (p != std::string::npos) cut = std::min(cut, p);
    }
    return s.substr(0, cut);
}

void MacroExpander::handleLine(const std::string& raw, int ln) {
    std::string s = trim(stripComment(raw));
    if (s.empty()) return;                       // blank/comment: does not break a batch

    size_t colon = s.find(':');
    if (colon != std::string::npos) {            // label ends any batch
        flush();
        emit(trim(s.substr(0, colon)) + ":", ln);
        s = trim(s.substr(colon + 1));
        if (s.empty()) return;
    }
    size_t sp = s.find_first_of(" \t");
    std::string mn   = lower(sp == std::string::npos ? s : s.substr(0, sp));
    std::string rest = sp == std::string::npos ? "" : trim(s.substr(sp));

    if (mn == "push" || mn == "pop") { handleStack(mn == "push", rest, ln); return; }
    flush();                                     // non-stack instruction ends the batch
    emit("    " + s, ln);
}

std::vector<int> MacroExpander::parseRegs(const std::string& args, bool isPush, int ln) {
    const std::string verb = isPush ? "push" : "pop";
    std::string s = args;
    for (char& c : s) if (c == ',') c = ' ';
    std::istringstream is(s);
    std::string tok;
    std::vector<int> regs;
    while (is >> tok) {
        tok = lower(tok);
        if (tok == "sp") fail(ln, "sp (r13) cannot be used in " + verb);
        bool ok = tok.size() >= 2 && tok.size() <= 4 && tok[0] == 'r';
        for (size_t i = 1; ok && i < tok.size(); ++i)
            ok = std::isdigit((unsigned char)tok[i]) != 0;
        if (!ok) fail(ln, "invalid register '" + tok + "' in " + verb);
        int n = std::stoi(tok.substr(1));
        if (n < 0 || n >= NUM_REGS)
            fail(ln, "register " + tok + " does not exist (r0..r15)");
        if (n == SP_REG)
            fail(ln, "r13 is the stack pointer and cannot be " +
                     std::string(isPush ? "pushed" : "popped"));
        if (!isPush && std::find(regs.begin(), regs.end(), n) != regs.end())
            fail(ln, "duplicate destination register " + tok + " in pop");
        regs.push_back(n);
    }
    if (regs.empty()) fail(ln, verb + " needs at least one register");
    return regs;
}

void MacroExpander::handleStack(bool isPush, const std::string& args, int ln) {
    std::vector<int> regs = parseRegs(args, isPush, ln);
    Kind k = isPush ? Kind::Push : Kind::Pop;

    if (!batching_) {                            // naive expansion, one SP update per reg
        flush();
        for (int r : regs) {
            if (isPush) {
                emit("    sub r13, r13, 4", ln);
                emit("    st " + R(r) + ", 0[r13]", ln);
            } else {
                emit("    ld " + R(r) + ", 0[r13]", ln);
                emit("    add r13, r13, 4", ln);
            }
            stats_.naiveInstr += 2;      stats_.emittedInstr += 2;
            stats_.naiveSpUpdates += 1;  stats_.emittedSpUpdates += 1;
        }
        return;
    }
    if (kind_ != k) flush();                     // push<->pop switch breaks the batch
    kind_ = k;
    for (int r : regs) {
        if ((pend_.size() + 1) * WORD > (size_t)MAX_IMM) { flush(); kind_ = k; }
        pend_.push_back({r, ln});
    }
}

void MacroExpander::flush() {
    if (pend_.empty()) { kind_ = Kind::None; return; }
    const size_t n     = pend_.size();
    const int    bytes = (int)(n * WORD);
    if (kind_ == Kind::Push) {
        // Attribute the combined SP decrement to the last push in the batch so
        // any stack-overflow fault reports the line that crossed STACK_BASE.
        emit("    sub r13, r13, " + std::to_string(bytes), pend_.back().line);
        for (size_t k = 0; k < n; ++k)           // first pushed reg ends highest in memory
            emit("    st " + R(pend_[k].reg) + ", " +
                     std::to_string((n - 1 - k) * WORD) + "[r13]", pend_[k].line);
    } else {
        for (size_t k = 0; k < n; ++k)           // first popped reg comes from the top (off 0)
            emit("    ld " + R(pend_[k].reg) + ", " +
                     std::to_string(k * WORD) + "[r13]", pend_[k].line);
        emit("    add r13, r13, " + std::to_string(bytes), pend_.back().line);
    }
    stats_.naiveInstr       += 2 * n;
    stats_.emittedInstr     += n + 1;
    stats_.naiveSpUpdates   += n;
    stats_.emittedSpUpdates += 1;
    stats_.batches++;
    pend_.clear();
    kind_ = Kind::None;
}

}  // namespace risc201
