#include "vm.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

#include "fault.hpp"
#include "stack_visualizer.hpp"

namespace risc201 {

VM::VM(StackGuard& g, std::ostream& os) : g_(g), os_(os), mem_(MEM_SIZE, 0) {
    R_.fill(0);
    isStackPtr_.fill(false);
    R_[SP_REG] = g.top();
    isStackPtr_[SP_REG] = true;
}

void VM::dumpStack() const {
    renderStack(os_, g_, [this](uint32_t a) { return rdWord(a); }, sp());
}

// ----------------------------------------------------------------------------
//  Loading / parsing
// ----------------------------------------------------------------------------
int VM::reg(const std::string& s, const AsmLine& al) {
    int n = std::stoi(s);
    if (n < 0 || n >= NUM_REGS)
        throw StackException(Fault::BadInstruction, "bad register r" + s, 0, al.srcLine);
    return n;
}

int64_t VM::parseImm(const std::string& s, const AsmLine& al) {
    try { return std::stoll(s, nullptr, 0); }
    catch (...) {
        throw StackException(Fault::BadInstruction, "bad immediate '" + s + "'", 0, al.srcLine);
    }
}

bool VM::isReg(const std::string& s) {
    if (s.size() < 2 || (s[0] != 'r' && s[0] != 'R')) return false;
    return std::all_of(s.begin() + 1, s.end(), [](char c) { return std::isdigit((unsigned char)c); });
}

void VM::operand(const std::string& s, Instr& in, const AsmLine& al) {
    if (isReg(s)) { in.useImm = false; in.rt = reg(s.substr(1), al); }
    else          { in.useImm = true;  in.imm = parseImm(s, al); }
}

void VM::load(const std::vector<AsmLine>& lines) {
    static const std::regex reAlu(R"(^(add|sub)\s+r(\d+)\s*,\s*r(\d+)\s*,\s*(\S+)$)", std::regex::icase);
    static const std::regex reMov(R"(^mov\s+r(\d+)\s*,\s*(\S+)$)", std::regex::icase);
    static const std::regex reMem(R"(^(ld|st)\s+r(\d+)\s*,\s*(-?\w+)\s*\[\s*r(\d+)\s*\]$)", std::regex::icase);
    static const std::regex reJmp(R"(^(call|jmp)\s+([A-Za-z_]\w*)$)", std::regex::icase);
    prog_.clear();
    labels_.clear();
    for (const AsmLine& al : lines) {
        std::string t = trim(al.text);
        if (t.empty()) continue;
        if (t.back() == ':') {
            std::string name = t.substr(0, t.size() - 1);
            if (labels_.count(name))
                throw StackException(Fault::BadInstruction,
                                     "duplicate label '" + name + "'", 0, al.srcLine);
            labels_[name] = prog_.size();
            continue;
        }
        Instr in;
        in.srcLine = al.srcLine;
        in.text    = t;
        std::smatch m;
        std::string lt = lower(t);
        if (std::regex_match(t, m, reAlu)) {
            in.op = lower(m[1]) == "add" ? Op::ADD : Op::SUB;
            in.rd = reg(m[2], al); in.rs = reg(m[3], al);
            operand(m[4], in, al);
        } else if (std::regex_match(t, m, reMov)) {
            in.op = Op::MOV; in.rd = reg(m[1], al); operand(m[2], in, al);
        } else if (std::regex_match(t, m, reMem)) {
            in.op = lower(m[1]) == "ld" ? Op::LD : Op::ST;
            in.rd = reg(m[2], al);                   // data register
            in.imm = parseImm(m[3], al);
            in.rs = reg(m[4], al);                   // base register
        } else if (std::regex_match(t, m, reJmp)) {
            in.op = lower(m[1]) == "call" ? Op::CALL : Op::JMP;
            in.label = m[2];
        } else if (lt == "ret")  in.op = Op::RET;
        else if (lt == "halt")   in.op = Op::HALT;
        else if (lt == "viz")    in.op = Op::VIZ;
        else throw StackException(Fault::BadInstruction,
                                  "cannot parse '" + t + "'", 0, al.srcLine);
        prog_.push_back(in);
    }
    for (Instr& in : prog_)
        if (in.op == Op::CALL || in.op == Op::JMP) {
            auto it = labels_.find(in.label);
            if (it == labels_.end())
                throw StackException(Fault::BadInstruction,
                                     "undefined label '" + in.label + "'", 0, in.srcLine);
            in.target = it->second;
        }
}

// ----------------------------------------------------------------------------
//  Execution
// ----------------------------------------------------------------------------
void VM::run(size_t maxSteps) {
    pc_ = 0;
    halted_ = false;
    for (size_t steps = 0; !halted_; ++steps) {
        if (steps >= maxSteps)
            throw StackException(Fault::BadInstruction, "step limit exceeded");
        if (pc_ >= prog_.size())
            throw StackException(Fault::BadInstruction,
                "pc " + std::to_string(pc_) + " is outside the program");
        const Instr& in = prog_[pc_];
        try {
            step(in);
        } catch (StackException& e) {
            e.srcLine = in.srcLine;
            e.pc      = (uint32_t)pc_;
            e.srcText = in.text;
            throw;
        }
    }
}

uint32_t VM::rdWord(uint32_t a) const {
    return (uint32_t)mem_[a] | (uint32_t)mem_[a + 1] << 8 |
           (uint32_t)mem_[a + 2] << 16 | (uint32_t)mem_[a + 3] << 24;
}

void VM::wrWord(uint32_t a, uint32_t v) {
    for (int i = 0; i < 4; ++i) mem_[a + i] = (uint8_t)(v >> (8 * i));
}

// Every write to a register goes through here so SP writes are verified first
// and stack-pointer provenance is tracked across register copies/offsets.
void VM::writeReg(int rd, int64_t v, bool stackDerived) {
    if (rd == SP_REG) g_.checkSP(v);
    R_[rd] = (uint32_t)v;
    isStackPtr_[rd] = (rd == SP_REG) || stackDerived;
}

void VM::step(const Instr& in) {
    size_t next = pc_ + 1;
    switch (in.op) {
        case Op::ADD: case Op::SUB: {
            int64_t a = R_[in.rs];
            int64_t b = in.useImm ? in.imm : (int64_t)R_[in.rt];
            bool pa = isStackPtr_[in.rs], pb = !in.useImm && isStackPtr_[in.rt];
            bool stackDerived = in.op == Op::ADD ? (pa || pb) : (pa && !pb);
            writeReg(in.rd, in.op == Op::ADD ? a + b : a - b, stackDerived);
            break;
        }
        case Op::MOV: {
            bool stackDerived = !in.useImm && isStackPtr_[in.rt];
            writeReg(in.rd, in.useImm ? in.imm : (int64_t)R_[in.rt], stackDerived);
            break;
        }
        case Op::LD: case Op::ST: {
            bool store = in.op == Op::ST;
            int64_t addr = (int64_t)R_[in.rs] + in.imm;
            bool viaSP = (in.rs == SP_REG) || isStackPtr_[in.rs];
            g_.checkAccess(addr, store, R_[SP_REG], viaSP);
            if (store) wrWord((uint32_t)addr, R_[in.rd]);
            else       writeReg(in.rd, rdWord((uint32_t)addr), false);
            break;
        }
        case Op::CALL: {
            uint32_t sp = R_[SP_REG];
            uint32_t retSlot = sp - WORD;
            g_.checkSP((int64_t)sp - (g_.canaryOn() ? 2 * WORD : WORD));
            wrWord(retSlot, (uint32_t)next);
            uint32_t c = g_.pushFrame(retSlot, (uint32_t)next);
            if (g_.canaryOn()) { wrWord(retSlot - WORD, c); R_[SP_REG] = retSlot - WORD; }
            else               { R_[SP_REG] = retSlot; }
            next = in.target;
            break;
        }
        case Op::RET: {
            uint32_t sp = R_[SP_REG];
            if (g_.frames().empty()) g_.verifyReturn(sp, 0, 0);       // throws
            const auto& f = g_.frames().back();
            if (sp != f.canarySlot) g_.verifyReturn(sp, 0, 0);        // throws (unbalanced)
            uint32_t canary = g_.canaryOn() ? rdWord(f.canarySlot) : 0;
            uint32_t ret    = rdWord(f.retSlot);
            uint32_t slotSP = f.retSlot + WORD;
            g_.verifyReturn(sp, canary, ret);                         // may throw
            R_[SP_REG] = slotSP;
            next = ret;
            break;
        }
        case Op::JMP:  next = in.target; break;
        case Op::HALT: halted_ = true; break;
        case Op::VIZ:  dumpStack(); break;
    }
    pc_ = next;
}

}  // namespace risc201
