// macro_expander.hpp -- (e) push/pop macro expansion for a FULL-DESCENDING stack
//
//  Naive  push r1,r2,r3 :  sub r13,r13,4 ; st r1,0[r13]   (x3)        6 instr
//  Batched              :  sub r13,r13,12 ; st r1,8[r13] ; st r2,4[r13]
//                          ; st r3,0[r13]                              4 instr
//  Batched pop r1,r2,r3 :  ld r1,0[r13] ; ld r2,4[r13] ; ld r3,8[r13]
//                          ; add r13,r13,12
//  A batch is broken by: a label, any non-stack instruction, push<->pop
//  switch, or when the total size would exceed the immediate range.
//  Validation: registers checked, r13/sp rejected, duplicate destination
//  registers in one pop rejected, source-line map kept for error messages.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace risc201 {

struct AsmLine {
    std::string text;
    int         srcLine;   // original source line this instruction came from
};

struct ExpandStats {
    size_t naiveInstr = 0, emittedInstr = 0;
    size_t naiveSpUpdates = 0, emittedSpUpdates = 0;
    size_t batches = 0;
};

class MacroExpander {
public:
    explicit MacroExpander(bool batching = true) : batching_(batching) {}

    std::vector<AsmLine> expand(const std::string& source);
    const ExpandStats& stats() const { return stats_; }

private:
    enum class Kind { None, Push, Pop };
    struct Item { int reg; int line; };

    bool                 batching_;
    Kind                 kind_ = Kind::None;
    std::vector<Item>    pend_;
    std::vector<AsmLine> out_;
    ExpandStats          stats_;

    [[noreturn]] static void fail(int ln, const std::string& msg);
    static std::string R(int r);
    static std::string stripComment(const std::string& s);

    void emit(const std::string& t, int line);
    void handleLine(const std::string& raw, int ln);
    std::vector<int> parseRegs(const std::string& args, bool isPush, int ln);
    void handleStack(bool isPush, const std::string& args, int ln);
    void flush();
};

}  // namespace risc201
