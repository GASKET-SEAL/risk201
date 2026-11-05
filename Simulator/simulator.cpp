// RISC201 pipeline simulator + interactive debugger.
//
// The pipeline stages themselves (IF, ID, RR, EX, MA, RW and the 4-stage
// wrapper) come from the control-unit module. This file is the DRIVER: it owns
// the pipeline registers, PC updates, forwarding, load-use stalls, branch
// resolution/flushes, statistics and the debugger.
//
//   6-stage : IF -> ID -> RR -> EX -> MA -> RW
//   4-stage : IF -> OF(ID+RR) -> EX -> MW(MA+RW)

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <set>
#include <fstream>
#include <memory>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <map>
#include <array>
#include <functional>

// Team modules
#include "../alu/include/alu.hpp"
#include "../control/include/control_packet.hpp"
#include "../control/include/microinstruction.hpp"
#include "../control/include/control_memory.hpp"
#include "../control/include/microprogram_sequencer.hpp"
#include "../control/include/pipeline_registers.hpp"
#include "../control/include/instruction_memory.hpp"
#include "../control/include/data_memory.hpp"
#include "../control/include/register_file.hpp"
#include "../control/include/instruction_fetch_stage.hpp"
#include "../control/include/instruction_decode_stage.hpp"
#include "../control/include/register_read_stage.hpp"
#include "../control/include/execute_stage.hpp"
#include "../control/include/memory_access_stage.hpp"
#include "../control/include/write_back_stage.hpp"
#include "../control/include/four_stage_components.hpp"

// Stack subsystem (macro expander, guard, faults, visualiser) and the assembler
// (so the simulator can take .asm/.s source directly: expand -> assemble -> run)
#include "../stack/include/common.hpp"
#include "../stack/include/fault.hpp"
#include "../stack/include/macro_expander.hpp"
#include "../stack/include/stack_guard.hpp"
#include "../stack/include/stack_visualizer.hpp"
#include "../assembler/include/assembler.hpp"

using namespace std;

namespace risc201 {

// SP_REG, STACK_BASE, STACK_TOP and MEM_SIZE come from stack/common.hpp.
// Data memory covers exactly [0, MEM_SIZE); the stack region is [STACK_BASE, STACK_TOP).
static constexpr uint32_t DATA_WORDS  = MEM_SIZE / 4;

// Formats a value as 0x%08x without touching cout's sticky state.
static string h32(uint32_t v) {
    ostringstream o;
    o << "0x" << hex << setw(8) << setfill('0') << v;
    return o.str();
}

enum class PipelineMode { SIX = 6, FOUR = 4 };

struct StackOptions {
    bool on     = true;    // stack subsystem: guard + stack-based CALL/RET (--no-stack = plain link-register ISA)
    bool shadow = true;    // shadow stack          (--no-shadow)
    bool canary = true;    // stack canary          (--no-canary)
    bool naive  = false;   // naive push/pop macros (--naive; default is batched)
};

struct SrcInfo { int line = -1; string text; };   // original source line + expanded instruction

struct Program {
    vector<uint32_t> words;
    map<uint32_t, SrcInfo> srcmap;                // instruction address -> source info (.asm input)
    bool from_source = false;
    ExpandStats mstats;                           // push/pop macro statistics
};

// hex file: one 32-bit word per line, '//' and '#' comments allowed
static vector<uint32_t> read_hex_words(const string& filepath) {
    ifstream in(filepath);
    if (!in) throw runtime_error("Cannot open hex file: " + filepath);
    vector<uint32_t> words;
    string line;
    int lineno = 0;
    while (getline(in, line)) {
        ++lineno;
        size_t c = line.find("//");
        if (c != string::npos) line.erase(c);
        c = line.find('#');
        if (c != string::npos) line.erase(c);
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        line = line.substr(b, e - b + 1);
        try {
            words.push_back((uint32_t)stoul(line, nullptr, 16));
        } catch (...) {
            throw runtime_error("Bad hex on line " + to_string(lineno) + ": " + line);
        }
    }
    return words;
}

static bool ends_with(const string& s, const string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// .asm/.s source:  push/pop macro expansion -> two-pass assembler -> machine words.
// Every machine word keeps the ORIGINAL source line it came from (for fault reports);
// assembler errors are reported against original source lines too.
static Program build_program(const string& path, const StackOptions& so) {
    Program prog;
    if (ends_with(path, ".asm") || ends_with(path, ".s")) {
        ifstream in(path);
        if (!in) throw runtime_error("Cannot open source file: " + path);
        stringstream buf;
        buf << in.rdbuf();

        MacroExpander mx(!so.naive);
        vector<AsmLine> lines = mx.expand(buf.str());       // may throw StackException(MacroError)
        prog.mstats = mx.stats();

        string text;                                        // expanded line N == lines[N-1]
        for (const AsmLine& l : lines) { text += l.text; text += '\n'; }

        Assembler assembler;
        AssembleResult r = assembler.assembleText(text);
        if (!r.success) {
            ostringstream err;
            for (const auto& d : r.diagnostics) {
                int idx = d.lineNumber - 1;
                int src = (idx >= 0 && idx < (int)lines.size()) ? lines[idx].srcLine : d.lineNumber;
                err << path << ":" << src << ": error: " << d.message << "\n";
            }
            err << r.diagnostics.size() << " error(s), nothing loaded";
            throw runtime_error(err.str());
        }
        for (const auto& e : r.words) {
            prog.words.push_back(e.machineWord);
            int idx = e.lineNumber - 1;
            if (idx >= 0 && idx < (int)lines.size())
                prog.srcmap[e.address] = {lines[idx].srcLine, trim(lines[idx].text)};
        }
        prog.from_source = true;
    } else {
        prog.words = read_hex_words(path);
    }
    return prog;
}

class Simulator {
public:
    struct MicroRow {
        uint16_t upc;
        ControlPacket ctrl;
    };

    struct Stats {
        uint64_t cycles = 0, retired = 0, stalls = 0, flushes = 0;
        double cpi() const { return retired ? (double)cycles / (double)retired : 0.0; }
    };

    // ---- hardware (declaration order == construction order) ----
    PipelineMode mode;
    StackOptions sopt;
    InstructionMemory imem;
    DataMemory dmem;
    RegisterFile rf;
    ControlMemory cmem;
    Alu alu;
    MicroProgramSequencer seq;          // used for the debugger's microPC view
    InstructionFetchStage s_if;
    InstructionDecodeStage s_id;
    RegisterReadStage s_rr;
    ExecuteStage s_ex;
    MemoryAccessStage s_ma;
    WriteBackStage s_wb;
    FourStageComponents four;

    // ---- pipeline registers ----
    // 6-stage: if_id, id_rr, rr_ex, ex_ma, ma_rw
    // 4-stage: if_id (IF/OF), rr_ex (OF/EX), ex_ma (EX/MW)
    IFIDRegister if_id;
    IDRRRegister id_rr;
    RREXRegister rr_ex;
    EXMARegister ex_ma;
    MARWRegister ma_rw;

    uint32_t pc = 0;
    AluFlags flags;
    bool halted = false;

    Stats stats;
    uint64_t max_cycles = 1000000;      // guards against infinite loops
    uint32_t prog_start = 0, prog_end = 0;
    string halt_reason;
    bool stalled_now = false;
    bool flushed_now = false;
    uint32_t last_opcode = 0;
    bool have_opcode = false;

    // ---- stack subsystem state ----
    unique_ptr<StackGuard> guard;
    array<bool, NUM_REGS> sp_derived{};     // "holds a pointer derived from SP" (per register)
    map<uint32_t, SrcInfo> srcmap;          // instruction address -> original source line
    unique_ptr<StackException> stack_fault;   // set when the guard stopped the machine

    Simulator(PipelineMode m, const AluConfig& cfg, const StackOptions& so)
        : mode(m), sopt(so), imem(), dmem(DATA_WORDS), rf(), cmem(), alu(cfg), seq(cmem),
          s_if(imem), s_id(cmem), s_rr(), s_ex(alu), s_ma(dmem), s_wb(rf),
          four(imem, cmem, alu, dmem, rf) {}

    // ------------------------------------------------------------------
    // Program loading
    // ------------------------------------------------------------------
    void load_program(const Program& prog) {
        imem.load(prog.words);
        srcmap = prog.srcmap;
        prog_start = 0;
        prog_end = (uint32_t)prog.words.size() * 4;
        pc = 0;
        halted = false;
        halt_reason.clear();
        stack_fault.reset();
        rf.reset();
        rf.write(SP_REG, STACK_TOP);            // full-descending: empty stack, SP = top
        dmem.reset();
        sp_derived.fill(false);
        sp_derived[SP_REG] = true;
        guard = make_unique<StackGuard>(STACK_BASE, STACK_TOP, sopt.shadow, sopt.canary);
    }

    // ------------------------------------------------------------------
    // Microprogram view (direct opcode lookup: one microinstruction/opcode)
    // ------------------------------------------------------------------
    vector<MicroRow> trace_micro(uint8_t opcode, string& err) {
        vector<MicroRow> rows;
        try {
            seq.start(opcode);
            rows.push_back({seq.micro_pc(), makeControlPacket(seq.current())});
        } catch (const exception& e) {
            err = e.what();
        }
        return rows;
    }

    uint16_t upc_of(uint32_t opcode) {
        try { seq.start((uint8_t)opcode); return seq.micro_pc(); }
        catch (...) { return 0; }
    }

    // ------------------------------------------------------------------
    // Helpers: which source registers does an instruction really read?
    // (Branch/CALL offset bits and unused fields must not cause hazards.)
    // ------------------------------------------------------------------
    static bool uses_rs1(const ControlPacket& c) {
        return (c.alu_control != ALUControl::NONE &&
                c.alu_control != ALUControl::MOV &&
                c.alu_control != ALUControl::NOT) ||
               c.branch_control == BranchControl::RET;
    }
    static bool uses_rs2(const ControlPacket& c) {
        return c.alu_control != ALUControl::NONE && !c.use_immediate;
    }
    static bool uses_rd_as_src(const ControlPacket& c) { return c.memory_write; }

    static bool depends_on(const ControlPacket& c, uint8_t rs1, uint8_t rs2, uint8_t rd,
                           uint8_t reg) {
        return (uses_rs1(c) && rs1 == reg) || (uses_rs2(c) && rs2 == reg) ||
               (uses_rd_as_src(c) && rd == reg);
    }

    static uint32_t ex_value(const EXMARegister& e) {
        return e.control.write_back == WriteBackControl::PC_PLUS_4 ? e.pc_plus_4 : e.alu_result;
    }
    static uint32_t wb_value(const MARWRegister& m) {
        switch (m.control.write_back) {
            case WriteBackControl::MEMORY_DATA: return m.memory_data;
            case WriteBackControl::PC_PLUS_4:   return m.pc_plus_4;
            default:                            return m.alu_result;
        }
    }

    // Forwarding: patch the operands of the instruction about to execute.
    // EX/MA has priority (newest value); MA/RW only exists in the 6-stage pipe.
    void forward(RREXRegister& in, bool use_ma_rw) const {
        auto fwd = [&](uint8_t reg, uint32_t& val) {
            if (ex_ma.valid && ex_ma.control.register_write && ex_ma.rd == reg) {
                val = ex_value(ex_ma);
            } else if (use_ma_rw && ma_rw.valid && ma_rw.control.register_write &&
                       ma_rw.rd == reg) {
                val = wb_value(ma_rw);
            }
        };
        if (uses_rs1(in.control))       fwd(in.rs1, in.operand1);
        if (uses_rs2(in.control))       fwd(in.rs2, in.operand2);
        if (uses_rd_as_src(in.control)) fwd(in.rd, in.store_data);
    }

    static string opcode_str(uint32_t instr) {
        ostringstream o;
        o << "(opcode " << (instr >> 27) << " = 0x" << hex << (instr >> 27) << ")";
        return o.str();
    }

    // ------------------------------------------------------------------
    // Stack subsystem hooks (StackGuard), called from the memory stage.
    //
    // The memory stage is the first point where every OLDER instruction has fully
    // retired its register and memory effects (RW runs before MA; stores perform in
    // MA), so SP and memory seen here are exactly the program-order state, and a
    // fault is precise: nothing younger has executed, nothing older is lost.
    //
    // CALL/RET follow the reference VM: CALL pushes the return address (and canary)
    // on the stack and registers a frame; RET verifies SP, canary and the shadow copy,
    // then pops. (The ISA's link register R15 is still written by CALL.)
    // Returns true and the target when a RET is resolved here.
    // ------------------------------------------------------------------
    bool stack_stage(const EXMARegister& e, uint32_t& ret_target) {
        const ControlPacket& c = e.control;
        const uint32_t w = imem.read(e.pc);                  // EX/MA does not carry rs1/rs2
        const int rs1 = (w >> 18) & 0xF, rs2 = (w >> 14) & 0xF;
        const uint32_t sp = rf.read(SP_REG);
        StackGuard& g = *guard;

        if (c.branch_control == BranchControl::CALL) {
            const uint32_t retSlot = sp - WORD;
            g.checkSP((int64_t)sp - (g.canaryOn() ? 2 * WORD : WORD));
            dmem.write(retSlot, e.pc_plus_4);
            const uint32_t canary = g.pushFrame(retSlot, e.pc_plus_4);
            uint32_t newSP = retSlot;
            if (g.canaryOn()) { dmem.write(retSlot - WORD, canary); newSP = retSlot - WORD; }
            rf.write(SP_REG, newSP);
            sp_derived[e.rd] = false;                        // R15 = return address, not a stack pointer
            return false;
        }

        if (c.branch_control == BranchControl::RET) {
            if (g.frames().empty()) g.verifyReturn(sp, 0, 0);          // throws ShadowStackEmpty
            const StackGuard::Frame f = g.frames().back();             // copy: verifyReturn pops it
            if (sp != f.canarySlot) g.verifyReturn(sp, 0, 0);          // throws UnbalancedFrame
            const uint32_t canary = g.canaryOn() ? dmem.read(f.canarySlot) : 0;
            const uint32_t ret    = dmem.read(f.retSlot);
            g.verifyReturn(sp, canary, ret);                           // may throw
            if (ret >= prog_end || (ret & 3u))
                throw StackException(Fault::BadInstruction,
                                     "return to invalid address " + h32(ret), ret);
            rf.write(SP_REG, f.retSlot + WORD);
            ret_target = ret;
            return true;
        }

        if (c.memory_read || c.memory_write) {
            const bool viaSP = (rs1 == (int)SP_REG) || sp_derived[rs1];
            g.checkAccess((int64_t)(int32_t)e.alu_result, c.memory_write, sp, viaSP);
            if (c.memory_read && c.register_write && e.rd == SP_REG)   // ld r13, ...
                g.checkSP((int64_t)dmem.read(e.alu_result));
        } else if (c.register_write && e.rd == SP_REG) {
            g.checkSP((int64_t)(int32_t)e.alu_result);                 // add/sub/mov ... r13
        }

        // SP-derived provenance (same rules as the reference VM; other ALU ops clear it)
        if (c.register_write) {
            bool derived = false;
            if (!c.memory_read) {
                const bool pa = sp_derived[rs1];
                const bool pb = !c.use_immediate && sp_derived[rs2];
                switch (c.alu_control) {
                    case ALUControl::ADD: derived = pa || pb; break;
                    case ALUControl::SUB: derived = pa && !pb; break;
                    case ALUControl::MOV: derived = pb; break;
                    default: break;
                }
            }
            sp_derived[e.rd] = (e.rd == (uint8_t)SP_REG) || derived;
        }
        return false;
    }

    void stack_halt(StackException& ex, uint32_t at_pc) {
        auto it = srcmap.find(at_pc);
        ex.pc = at_pc / 4;                                   // instruction index, like the reference VM
        if (it != srcmap.end()) { ex.srcLine = it->second.line; ex.srcText = it->second.text; }
        stack_fault.reset(new StackException(ex));
        halted = true;
        halt_reason = string("EXCEPTION ") + faultName(ex.fault) + ": " + ex.what();
    }

    // Runs the hook for the instruction entering the memory stage.
    // Returns false if a fault stopped the machine.
    bool run_stack_hook(const EXMARegister& e, bool& redirect, uint32_t& target) {
        redirect = false;
        if (!sopt.on || !e.valid) return true;
        try {
            redirect = stack_stage(e, target);
        } catch (StackException& ex) {
            stack_halt(ex, e.pc);
            return false;
        } catch (const exception& ex) {
            raise_fault(string(ex.what()) + " at PC " + h32(e.pc));
            return false;
        }
        return true;
    }

    void raise_fault(const string& msg) {
        halted = true;
        halt_reason = "EXCEPTION: " + msg;
    }

    bool pipeline_empty() const {
        if (mode == PipelineMode::FOUR) return !if_id.valid && !rr_ex.valid && !ex_ma.valid;
        return !if_id.valid && !id_rr.valid && !rr_ex.valid && !ex_ma.valid && !ma_rw.valid;
    }

    // EX stage + branch resolution (shared by both pipelines).
    // Returns false if a fault stopped the machine.
    bool execute_stage(const RREXRegister& in, bool& flush) {
        ex_ma = EXMARegister{};
        if (!in.valid) return true;
        try {
            ex_ma = (mode == PipelineMode::FOUR) ? four.execute(in, flags)
                                                 : s_ex.execute(in, flags);
        } catch (const exception& e) {
            raise_fault(string(e.what()) + " at PC " + h32(in.pc));
            ex_ma = EXMARegister{};
            return false;
        }
        if (in.control.flag_write) flags = ex_ma.flags;

        uint32_t rel = in.pc + 4 + (uint32_t)(in.immediate * 4);
        bool taken = false;
        uint32_t target = 0;
        switch (in.control.branch_control) {
            case BranchControl::B:
            case BranchControl::CALL: taken = true; target = rel; break;
            case BranchControl::BEQ:  if (flags.E)  { taken = true; target = rel; } break;
            case BranchControl::BGT:  if (flags.GT) { taken = true; target = rel; } break;
            case BranchControl::RET:   // stack mode: target comes from the stack, resolved at MA
                if (!sopt.on) { taken = true; target = in.operand1; }
                break;
            default: break;
        }
        if (taken) { pc = target; flush = true; }
        return true;
    }

    bool fetch_stage() {
        if (pc < prog_end) {
            if (pc & 3u) {
                raise_fault("unaligned instruction fetch at " + h32(pc));
                return false;
            }
            if_id = s_if.fetch(pc);
            pc += 4;
        } else {
            if_id = IFIDRegister{};
        }
        return true;
    }

    // ------------------------------------------------------------------
    // One clock cycle
    // ------------------------------------------------------------------
    void step() {
        if (halted) return;
        if (stats.cycles >= max_cycles) {
            halted = true;
            halt_reason = "cycle limit (" + to_string(max_cycles) + ") reached - possible infinite loop";
            return;
        }
        stats.cycles++;
        stalled_now = false;
        flushed_now = false;
        if (mode == PipelineMode::SIX) step6(); else step4();

        if (!halted && pipeline_empty() && pc >= prog_end) {
            halted = true;
            halt_reason = "end of program reached (no HALT executed)";
        }
    }

    // Stages run back to front so each stage consumes its input register
    // before the stage behind it overwrites it (like one common clock edge).
    void step6() {
        // ---- hazard unit (reads start-of-cycle registers) ----
        RREXRegister ex_in = rr_ex;
        if (ex_in.valid) forward(ex_in, true);

        bool stall = rr_ex.valid && rr_ex.control.memory_read && rr_ex.control.register_write &&
                     id_rr.valid &&
                     depends_on(id_rr.control, id_rr.rs1, id_rr.rs2, id_rr.rd, rr_ex.rd);
        if (stall) { stats.stalls++; stalled_now = true; }
        bool flush = false;

        // ---- RW ----
        if (ma_rw.valid) {
            stats.retired++;
            if (ma_rw.control.halt) { halted = true; halt_reason = "HALT"; return; }
            s_wb.execute(ma_rw);
        }

        // ---- MA ----
        bool ma_redirect = false;
        uint32_t ma_target = 0;
        if (!run_stack_hook(ex_ma, ma_redirect, ma_target)) { ma_rw = MARWRegister{}; return; }
        try {
            ma_rw = s_ma.execute(ex_ma);
        } catch (const exception& e) {
            raise_fault(string(e.what()) + " (address " + h32(ex_ma.alu_result) +
                        ", PC " + h32(ex_ma.pc) + ")");
            ma_rw = MARWRegister{};
            return;
        }
        if (ma_redirect) {                 // RET resolved from the stack: squash everything younger
            pc = ma_target;
            ex_in = RREXRegister{};
            id_rr = IDRRRegister{};
            if_id = IFIDRegister{};
            stall = false;
            flush = true;
        }

        // ---- EX ----
        if (!execute_stage(ex_in, flush)) return;

        // ---- RR ----
        rr_ex = stall ? RREXRegister{} : s_rr.execute(id_rr, rf);

        // ---- ID ----
        if (!stall) {
            try {
                id_rr = s_id.decode(if_id);
            } catch (const exception& e) {
                raise_fault(string(e.what()) + " " + opcode_str(if_id.instruction) +
                            " at PC " + h32(if_id.pc));
                id_rr = IDRRRegister{};
                return;
            }
            if (id_rr.valid) { last_opcode = id_rr.instruction >> 27; have_opcode = true; }
        }

        // ---- IF ----
        if (!stall && !fetch_stage()) return;

        if (flush) {
            rr_ex = RREXRegister{};
            id_rr = IDRRRegister{};
            stats.flushes++;
            flushed_now = true;
        }
    }

    // 4-stage: IF -> OF -> EX -> MW
    void step4() {
        // ---- hazard unit ----
        RREXRegister ex_in = rr_ex;
        if (ex_in.valid) forward(ex_in, false);   // only EX/MW -> EX path exists

        // Peek at the instruction about to be decoded (fields only; the
        // register values are re-read after MW has written back).
        RREXRegister peek;
        if (if_id.valid) {
            try {
                peek = four.operandFetch(if_id);
            } catch (const exception& e) {
                raise_fault(string(e.what()) + " " + opcode_str(if_id.instruction) +
                            " at PC " + h32(if_id.pc));
                return;
            }
        }
        bool stall = rr_ex.valid && rr_ex.control.memory_read && rr_ex.control.register_write &&
                     peek.valid &&
                     depends_on(peek.control, peek.rs1, peek.rs2, peek.rd, rr_ex.rd);
        if (stall) { stats.stalls++; stalled_now = true; }
        bool flush = false;

        // ---- MW (memory access + write-back) ----
        bool ma_redirect = false;
        uint32_t ma_target = 0;
        if (ex_ma.valid) {
            if (!run_stack_hook(ex_ma, ma_redirect, ma_target)) return;
            stats.retired++;
            if (ex_ma.control.halt) { halted = true; halt_reason = "HALT"; return; }
            try {
                four.memoryWriteback(ex_ma);
            } catch (const exception& e) {
                raise_fault(string(e.what()) + " (address " + h32(ex_ma.alu_result) +
                            ", PC " + h32(ex_ma.pc) + ")");
                return;
            }
        }
        if (ma_redirect) {                 // RET resolved from the stack: squash everything younger
            pc = ma_target;
            ex_in = RREXRegister{};
            if_id = IFIDRegister{};
            stall = false;
            flush = true;
        }

        // ---- EX ----
        if (!execute_stage(ex_in, flush)) return;

        // ---- OF (decode + register read; sees the write-back above) ----
        if (stall) {
            rr_ex = RREXRegister{};
        } else {
            rr_ex = four.operandFetch(if_id);
            if (if_id.valid) { last_opcode = if_id.instruction >> 27; have_opcode = true; }
        }

        // ---- IF ----
        if (!stall && !fetch_stage()) return;

        if (flush) {
            rr_ex = RREXRegister{};
            stats.flushes++;
            flushed_now = true;
        }
    }
};

// ======================================================================
// Debugger
// ======================================================================
class Debugger {
    unique_ptr<Simulator> sim;
    set<uint32_t> breakpoints;
    string src_path;
    Program prog;
    PipelineMode mode;
    AluConfig cfg;
    StackOptions sopt;

    static string stage_pc(bool valid, uint32_t pc) {
        return valid ? h32(pc) : string("   ----   ");
    }

    string alu_desc() const {
        if (cfg.reference) return "reference";
        return string(toString(cfg.adder)) + "/" + toString(cfg.mul) + "/" + toString(cfg.div);
    }

    void print_state() {
        Simulator& s = *sim;
        bool six = (s.mode == PipelineMode::SIX);
        cout << "\n=== Cycle " << dec << s.stats.cycles << " (" << (six ? 6 : 4) << "-stage) ===";
        if (s.stalled_now) cout << "  [STALL: load-use hazard]";
        if (s.flushed_now) cout << "  [FLUSH: branch taken]";
        cout << "\n";

        cout << "Stage | PC         | Detail\n";
        cout << "------+------------+--------------------------------\n";

        auto opcode_info = [&](uint32_t instr) {
            uint32_t op = instr >> 27;
            cout << "  opcode 0x" << hex << op << "  uPC=0x" << s.upc_of(op) << dec;
        };

        cout << (six ? "IF/ID" : "IF/OF") << " | " << stage_pc(s.if_id.valid, s.if_id.pc)
             << " | instr " << (s.if_id.valid ? h32(s.if_id.instruction) : "-");
        if (!six && s.if_id.valid) opcode_info(s.if_id.instruction);
        cout << "\n";

        if (six) {
            cout << "ID/RR | " << stage_pc(s.id_rr.valid, s.id_rr.pc) << " | ";
            if (s.id_rr.valid) {
                opcode_info(s.id_rr.instruction);
                cout << "  rd R" << (int)s.id_rr.rd << "  rs1 R" << (int)s.id_rr.rs1
                     << "  rs2 R" << (int)s.id_rr.rs2 << "  I=" << ((s.id_rr.instruction >> 26) & 1);
            }
            cout << "\n";
        }

        cout << (six ? "RR/EX" : "OF/EX") << " | " << stage_pc(s.rr_ex.valid, s.rr_ex.pc)
             << " | val1 " << h32(s.rr_ex.valid ? s.rr_ex.operand1 : 0)
             << "  val2 " << h32(s.rr_ex.valid ? s.rr_ex.operand2 : 0) << "\n";
        cout << (six ? "EX/MA" : "EX/MW") << " | " << stage_pc(s.ex_ma.valid, s.ex_ma.pc)
             << " | ALU result " << h32(s.ex_ma.valid ? s.ex_ma.alu_result : 0) << "\n";
        if (six) {
            cout << "MA/RW | " << stage_pc(s.ma_rw.valid, s.ma_rw.pc)
                 << " | write data " << h32(s.ma_rw.valid ? Simulator::wb_value(s.ma_rw) : 0) << "\n";
        }

        cout << "\nPC: " << h32(s.pc) << "   Flags: E=" << s.flags.E << " GT=" << s.flags.GT << "\n";
        for (int i = 0; i < 16; i += 4) {
            for (int j = 0; j < 4; ++j) {
                cout << "R" << dec << left << setw(2) << (i + j) << right << "="
                     << h32(s.rf.read(i + j)) << "  ";
            }
            cout << "\n";
        }
        cout << "=====================\n";
        if (s.halted) {
            cout << "Program stopped: " << s.halt_reason << "\n";
            if (s.stack_fault) print_stack_fault(*s.stack_fault, false);
            print_summary();
        }
    }

    // Same layout as the reference harness: fault name, message, original source line.
    void print_stack_fault(const StackException& e, bool heading = true) {
        if (heading) cout << "EXCEPTION " << faultName(e.fault) << ": " << e.what() << "\n";
        if (!e.srcText.empty())
            cout << "  at source line " << e.srcLine << ", instruction #" << e.pc
                 << " `" << e.srcText << "`\n";
        else if (e.srcLine >= 0)
            cout << "  at source line " << e.srcLine << "\n";
        else
            cout << "  at instruction #" << e.pc << " (PC " << h32(e.pc * 4) << ")\n";
        if (e.fault != Fault::MacroError) dump_stack();     // automatic stack dump
    }

    void dump_stack() {
        if (!sim->sopt.on) { cout << "Stack subsystem disabled (--no-stack).\n"; return; }
        renderStack(cout, *sim->guard,
                    [this](uint32_t a) { return sim->dmem.read(a); }, sim->rf.read(SP_REG));
    }

    void print_macro_stats() {
        const ExpandStats& st = prog.mstats;
        if (!prog.from_source) { cout << "Macro stats: n/a (hex input, no source).\n"; return; }
        if (!st.naiveInstr) { cout << "Macro stats: no push/pop in this program.\n"; return; }
        cout << "macro stats: " << st.emittedInstr << " instr emitted vs " << st.naiveInstr
             << " naive, " << st.emittedSpUpdates << " SP update(s) vs " << st.naiveSpUpdates
             << " naive, " << st.batches << " batch(es)"
             << (sopt.naive ? "  [naive expansion]" : "") << "\n";
    }

    void print_summary() {
        Simulator& s = *sim;
        cout << "\n=== Execution Summary ===\n";
        cout << "Pipeline:        " << (s.mode == PipelineMode::SIX ? 6 : 4) << "-stage\n";
        cout << "ALU:             " << alu_desc() << "\n";
        cout << "Total Cycles:    " << s.stats.cycles << "\n";
        cout << "Instructions:    " << s.stats.retired << "\n";
        cout << "CPI:             " << fixed << setprecision(3) << s.stats.cpi() << defaultfloat << "\n";
        cout << "Branch Flushes:  " << s.stats.flushes << "\n";
        cout << "Load-Use Stalls: " << s.stats.stalls << "\n";
        cout << "=========================\n";
    }

    static void print_ctrl(const ControlPacket& c) {
        cout << "mem_rd=" << (int)c.memory_read
             << " mem_wr=" << (int)c.memory_write
             << " reg_wr=" << (int)c.register_write
             << " flag_wr=" << (int)c.flag_write
             << " imm=" << (int)c.use_immediate
             << " alu=" << (int)c.alu_control
             << " branch=" << (int)c.branch_control
             << " wb=" << (int)c.write_back
             << " halt=" << (int)c.halt;
    }

    void print_micro(uint8_t opcode) {
        string err;
        auto rows = sim->trace_micro(opcode, err);
        cout << "\n=== Microprogram trace: opcode 0x" << hex << (unsigned)opcode << dec << " ===\n";
        for (size_t i = 0; i < rows.size(); ++i) {
            cout << "step " << i << " | uPC 0x" << hex << rows[i].upc << dec << " | ";
            print_ctrl(rows[i].ctrl);
            cout << "\n";
        }
        if (!err.empty()) cout << "MICROCODE EXCEPTION: " << err << "\n";
    }

    void load_fresh() {
        sim = make_unique<Simulator>(mode, cfg, sopt);
        sim->load_program(prog);
    }

    // Run up to n cycles (n==0: until halt/breakpoint). Returns true on breakpoint.
    bool run_cycles(uint64_t n) {
        uint64_t done = 0;
        bool first = true;
        while (!sim->halted && (n == 0 || done < n)) {
            if (!first && sim->rr_ex.valid && breakpoints.count(sim->rr_ex.pc)) {
                cout << "Breakpoint hit at " << h32(sim->rr_ex.pc) << " (about to execute)\n";
                return true;
            }
            sim->step();
            ++done;
            first = false;
        }
        return false;
    }

public:
    Debugger(PipelineMode m, const AluConfig& c, const StackOptions& so) : mode(m), cfg(c), sopt(so) {}

    // Non-interactive: run to completion, print one machine-readable line.
    int batch(const string& filepath) {
        try {
            src_path = filepath;
            prog = build_program(src_path, sopt);
            load_fresh();
            while (!sim->halted) sim->step();
            Simulator& s = *sim;
            cout << "pipeline=" << (s.mode == PipelineMode::SIX ? 6 : 4)
                 << " alu=" << alu_desc()
                 << " cycles=" << s.stats.cycles
                 << " instructions=" << s.stats.retired
                 << " cpi=" << fixed << setprecision(3) << s.stats.cpi() << defaultfloat
                 << " flushes=" << s.stats.flushes
                 << " stalls=" << s.stats.stalls
                 << " stop=\"" << s.halt_reason << "\"";
            if (s.stack_fault && s.stack_fault->srcLine >= 0)
                cout << " srcline=" << s.stack_fault->srcLine;
            for (int r = 0; r < 16; ++r) cout << " r" << r << "=" << s.rf.read(r);
            cout << "\n";
            return 0;
        } catch (const StackException& e) {
            cout << "stop=\"EXCEPTION " << faultName(e.fault) << ": " << e.what() << "\" srcline=" << e.srcLine << "\n";
            return 0;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    void attach(const string& filepath) {
        try {
            src_path = filepath;
            prog = build_program(src_path, sopt);
            load_fresh();
            cout << "Loaded program from " << filepath << " ("
                 << (sim->prog_end - sim->prog_start) / 4 << " words)  ["
                 << (mode == PipelineMode::SIX ? 6 : 4) << "-stage, ALU: " << alu_desc()
                 << ", stack: " << (sopt.on ? "on" : "off")
                 << (sopt.on && !sopt.shadow ? " no-shadow" : "")
                 << (sopt.on && !sopt.canary ? " no-canary" : "") << "]\n";
            if (prog.from_source) print_macro_stats();
            repl();
        } catch (const StackException& e) {
            cerr << "EXCEPTION " << faultName(e.fault) << ": " << e.what() << "\n";
            if (e.srcLine >= 0) cerr << "  at source line " << e.srcLine << "\n";
        } catch (const exception& e) {
            cerr << "Startup Error: " << e.what() << "\n";
        }
    }

    void repl() {
        string line;
        cout << "RISC201 Debugger. Type 'help' for commands.\n";

        while (true) {
            cout << "(dbg) " << flush;
            if (!getline(cin, line)) break;

            istringstream iss(line);
            string cmd;
            iss >> cmd;
            if (cmd.empty()) continue;

            if (cmd == "help" || cmd == "h") {
                cout << "Commands:\n"
                     << "  step [N]   (s)      run N cycles (default 1)\n"
                     << "  run        (r, c)   run until halt or breakpoint\n"
                     << "  break <hex_addr> (b) set breakpoint (stops before that instr executes)\n"
                     << "  delete <hex_addr|all> (d)  remove breakpoint(s)\n"
                     << "  info break (i)      list breakpoints\n"
                     << "  regs                show pipeline + registers\n"
                     << "  print <Rn|hex_addr> (p)  show register or data-memory word\n"
                     << "  mem <hex_addr> [N] (x)   dump N data-memory words (default 8)\n"
                     << "  micro [hex_opcode] (m)   control-memory entry (default: last decoded opcode)\n"
                     << "  stack               draw the stack frames (ASCII)\n"
                     << "  macro               push/pop macro expansion statistics\n"
                     << "  reset               reload program from file\n"
                     << "  quit       (q)\n";
            } else if (cmd == "step" || cmd == "s") {
                uint64_t n = 1;
                if (!(iss >> n) || n == 0) n = 1;
                if (sim->halted) {
                    cout << "Program already stopped: " << sim->halt_reason << "\n";
                } else {
                    run_cycles(n);
                    print_state();
                }
            } else if (cmd == "run" || cmd == "r" || cmd == "continue" || cmd == "c") {
                if (sim->halted) {
                    cout << "Program already stopped: " << sim->halt_reason << "\n";
                } else {
                    run_cycles(0);
                    print_state();
                }
            } else if (cmd == "break" || cmd == "b") {
                uint32_t addr;
                if (iss >> hex >> addr) {
                    if (addr & 3u) {
                        cout << "Address must be 4-byte aligned.\n";
                    } else {
                        breakpoints.insert(addr);
                        cout << "Breakpoint set at " << h32(addr) << "\n";
                    }
                } else {
                    cout << "Usage: break <hex_addr>\n";
                }
            } else if (cmd == "delete" || cmd == "d") {
                string arg;
                if (iss >> arg) {
                    if (arg == "all") {
                        breakpoints.clear();
                        cout << "All breakpoints removed.\n";
                    } else {
                        try {
                            uint32_t addr = (uint32_t)stoul(arg, nullptr, 16);
                            if (breakpoints.erase(addr)) cout << "Removed " << h32(addr) << "\n";
                            else cout << "No breakpoint at " << h32(addr) << "\n";
                        } catch (...) { cout << "Invalid address.\n"; }
                    }
                } else {
                    cout << "Usage: delete <hex_addr|all>\n";
                }
            } else if (cmd == "info" || cmd == "i") {
                if (breakpoints.empty()) cout << "No breakpoints.\n";
                else for (uint32_t b : breakpoints) cout << "  " << h32(b) << "\n";
            } else if (cmd == "regs") {
                print_state();
            } else if (cmd == "micro" || cmd == "m") {
                unsigned op = 0;
                if (iss >> hex >> op) {
                    print_micro((uint8_t)op);
                } else if (sim->have_opcode) {
                    print_micro((uint8_t)sim->last_opcode);
                } else {
                    cout << "No instruction decoded yet. Usage: micro <hex_opcode>\n";
                }
            } else if (cmd == "stack" || cmd == "k") {
                dump_stack();
            } else if (cmd == "macro") {
                print_macro_stats();
            } else if (cmd == "reset") {
                try {
                    load_fresh();
                    cout << "Program reloaded. Breakpoints kept.\n";
                } catch (const exception& e) {
                    cout << "Reset failed: " << e.what() << "\n";
                }
            } else if (cmd == "print" || cmd == "p") {
                string target;
                if (iss >> target) {
                    try {
                        if (target[0] == 'r' || target[0] == 'R') {
                            int reg = stoi(target.substr(1));
                            if (reg >= 0 && reg < 16) cout << target << " = " << h32(sim->rf.read(reg)) << "\n";
                            else cout << "Register must be R0-R15.\n";
                        } else {
                            uint32_t addr = (uint32_t)stoul(target, nullptr, 16);
                            cout << "Mem[" << h32(addr) << "] = " << h32(sim->dmem.read(addr)) << "\n";
                        }
                    } catch (const exception& e) { cout << "Cannot read: " << e.what() << "\n"; }
                } else {
                    cout << "Usage: print <Rn|hex_addr>\n";
                }
            } else if (cmd == "mem" || cmd == "x") {
                uint32_t addr;
                uint32_t n = 8;
                if (iss >> hex >> addr) {
                    iss >> dec >> n;
                    if (n == 0 || n > 4096) n = 8;
                    for (uint32_t i = 0; i < n; ++i) {
                        uint32_t a = addr + i * 4;
                        try {
                            cout << h32(a) << ": " << h32(sim->dmem.read(a)) << "\n";
                        } catch (const exception& e) {
                            cout << h32(a) << ": <" << e.what() << ">\n";
                            break;
                        }
                    }
                } else {
                    cout << "Usage: mem <hex_addr> [N]\n";
                }
            } else if (cmd == "quit" || cmd == "q") {
                break;
            } else {
                cout << "Unknown command. Type 'help'.\n";
            }
        }
    }
};

} // namespace risc201

static void usage() {
    cerr << "Usage: risc201sim [options] <program.hex>\n"
         << "  --pipeline 4|6            pipeline depth (default 6)\n"
         << "  --alu-ref                 use the reference ALU instead of the structural one\n"
         << "  --adder ripple|cla|kogge  adder algorithm (default kogge)\n"
         << "  --mul shiftadd|booth      multiplier (default booth)\n"
         << "  --div restoring|nonrestoring  divider (default nonrestoring)\n"
         << "  --batch                   run to completion, print one summary line\n"
         << "  --naive                   naive push/pop expansion (default: batched)\n"
         << "  --no-shadow               disable the shadow stack\n"
         << "  --no-canary               disable the stack canary\n"
         << "  --no-stack                disable the stack subsystem (plain link-register CALL/RET)\n"
         << "  <program>                 .asm/.s source (macros expanded, assembled) or .hex\n";
}

int main(int argc, char** argv) {
    using namespace risc201;
    PipelineMode mode = PipelineMode::SIX;
    AluConfig cfg;
    cfg.reference = false;
    cfg.adder = AdderKind::KoggeStone;
    cfg.mul = MulKind::Booth4;
    cfg.div = DivKind::NonRestoring;
    bool batch = false;
    StackOptions sopt;
    string path;

    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        auto next = [&]() -> string { return (i + 1 < argc) ? string(argv[++i]) : string(); };
        if (a == "--pipeline") {
            string v = next();
            if (v == "4") mode = PipelineMode::FOUR;
            else if (v == "6") mode = PipelineMode::SIX;
            else { usage(); return 1; }
        } else if (a == "--alu-ref") {
            cfg.reference = true;
        } else if (a == "--adder") {
            string v = next();
            if (v == "ripple") cfg.adder = AdderKind::Ripple;
            else if (v == "cla") cfg.adder = AdderKind::Lookahead;
            else if (v == "kogge") cfg.adder = AdderKind::KoggeStone;
            else { usage(); return 1; }
        } else if (a == "--mul") {
            string v = next();
            if (v == "shiftadd") cfg.mul = MulKind::ShiftAdd;
            else if (v == "booth") cfg.mul = MulKind::Booth4;
            else { usage(); return 1; }
        } else if (a == "--div") {
            string v = next();
            if (v == "restoring") cfg.div = DivKind::Restoring;
            else if (v == "nonrestoring") cfg.div = DivKind::NonRestoring;
            else { usage(); return 1; }
        } else if (a == "--batch") {
            batch = true;
        } else if (a == "--naive") {
            sopt.naive = true;
        } else if (a == "--no-shadow") {
            sopt.shadow = false;
        } else if (a == "--no-canary") {
            sopt.canary = false;
        } else if (a == "--no-stack") {
            sopt.on = false;
        } else if (!a.empty() && a[0] == '-') {
            usage(); return 1;
        } else {
            path = a;
        }
    }
    if (path.empty()) { usage(); return 1; }

    Debugger dbg(mode, cfg, sopt);
    if (batch) return dbg.batch(path);
    dbg.attach(path);
    return 0;
}
