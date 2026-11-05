// vm.hpp -- minimal VM used as a test harness for the stack subsystem.
// In your own repo, call the StackGuard hooks from your CPU's execute stage
// instead of using this class.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "common.hpp"
#include "macro_expander.hpp"
#include "stack_guard.hpp"

namespace risc201 {

class VM {
public:
    VM(StackGuard& g, std::ostream& os);

    void load(const std::vector<AsmLine>& lines);
    void run(size_t maxSteps = 100000);

    uint32_t reg(int i) const { return R_[i]; }
    uint32_t sp() const { return R_[SP_REG]; }
    size_t programSize() const { return prog_.size(); }
    void dumpStack() const;

private:
    enum class Op { ADD, SUB, MOV, LD, ST, CALL, RET, JMP, HALT, VIZ };
    struct Instr {
        Op op = Op::HALT;
        int rd = 0, rs = 0, rt = 0;
        int64_t imm = 0;
        bool useImm = true;
        std::string label;
        size_t target = 0;
        int srcLine = 0;
        std::string text;
    };

    StackGuard&                    g_;
    std::ostream&                  os_;
    std::vector<uint8_t>           mem_;
    std::array<uint32_t, NUM_REGS> R_;
    std::array<bool, NUM_REGS>     isStackPtr_;   // stack-pointer provenance per register
    std::vector<Instr>             prog_;
    std::map<std::string, size_t>  labels_;
    size_t                         pc_ = 0;
    bool                           halted_ = false;

    static int     reg(const std::string& s, const AsmLine& al);
    static int64_t parseImm(const std::string& s, const AsmLine& al);
    static bool    isReg(const std::string& s);
    static void    operand(const std::string& s, Instr& in, const AsmLine& al);

    uint32_t rdWord(uint32_t a) const;
    void     wrWord(uint32_t a, uint32_t v);
    void     writeReg(int rd, int64_t v, bool stackDerived = false);
    void     step(const Instr& in);
};

}  // namespace risc201
