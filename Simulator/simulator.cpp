#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <iomanip>
#include <set>
#include <fstream>
#include <memory>
#include <cstdint>
#include <stdexcept>

// Team Modules
#include "../alu/include/alu.hpp"
#include "../control/include/control_packet.hpp"
#include "../control/include/microprogram_sequencer.hpp"
#include "../control/include/control_memory.hpp"
#include "../control/include/microinstruction.hpp"

using namespace std;

namespace risc201 {

// TODO: move HALT into the control packet (e.g. a `halt` field) so it is
// microcode-driven. Until then, keep the magic number in ONE named place.
static constexpr uint32_t OPC_HALT = 0b10100;
static constexpr uint32_t LINK_REG = 15;

// Formats a value as 0x%08x without touching cout's sticky state.
static string hex32(uint32_t v) {
    ostringstream o;
    o << "0x" << hex << setw(8) << setfill('0') << v;
    return o.str();
}

class Simulator {
public:
    // --- PIPELINE BUFFERS ---
    struct IF_ID {
        uint32_t pc = 0, instr = 0;
        bool valid = false;
    };

    struct ID_RR {
        uint32_t pc = 0, opcode = 0, rd = 0, rs1 = 0, rs2 = 0;
        uint32_t I = 0;
        int32_t imm18 = 0, branch_offset = 0;
        uint16_t micro_pc = 0;   // microPC of this instruction's microprogram entry
        ControlPacket ctrl;
        bool valid = false;
    };

    struct RR_EX {
        uint32_t pc = 0, opcode = 0, rd = 0, rs1 = 0, rs2 = 0;
        uint32_t I = 0;
        int32_t val1 = 0, val2 = 0, val3 = 0;
        int32_t imm18 = 0, branch_offset = 0;
        ControlPacket ctrl;
        bool valid = false;
    };

    struct EX_MA {
        uint32_t pc = 0, opcode = 0, rd = 0;
        int32_t alu_result = 0, val3 = 0;
        ControlPacket ctrl;
        bool valid = false;
    };

    struct MA_RW {
        uint32_t pc = 0, opcode = 0, rd = 0;
        int32_t write_data = 0;
        ControlPacket ctrl;
        bool valid = false;
    };

    struct MicroRow {
        uint16_t upc;
        ControlPacket ctrl;
    };

    struct Stats {
        uint64_t cycles = 0, retired = 0, stalls = 0, flushes = 0;
        double cpi() const { return retired ? (double)cycles / (double)retired : 0.0; }
    };

    IF_ID if_id;
    ID_RR id_rr;
    RR_EX rr_ex;
    EX_MA ex_ma;
    MA_RW ma_rw;

    uint32_t pc;
    vector<uint32_t> regs;
    unordered_map<uint32_t, uint32_t> memory;
    AluFlags flags;
    Alu alu;
    ControlMemory ctrl_mem;
    MicroProgramSequencer sequencer;
    bool halted;

    // --- new state ---
    Stats stats;
    uint64_t max_cycles = 1000000;     // guards against infinite loops
    uint32_t prog_start = 0, prog_end = 0;  // [start, end) of loaded program
    string halt_reason;                // why we stopped (HALT, fault, limit...)
    bool stalled_now = false;          // did the last step() stall?
    bool flushed_now = false;          // did the last step() flush?
    uint32_t last_opcode = 0;          // last opcode sent to the sequencer
    bool have_opcode = false;

    Simulator() : pc(0), regs(16, 0), flags{false, false}, sequencer(ctrl_mem), halted(false) {
        AluConfig cfg;
        cfg.reference = true;
        alu = Alu(cfg);
    }

    void load_memory_from_hex(const string& filepath, uint32_t start_addr = 0) {
        ifstream in(filepath);
        if (!in) throw runtime_error("Cannot open hex file: " + filepath);

        string line;
        uint32_t addr = start_addr;
        int lineno = 0;
        while (getline(in, line)) {
            ++lineno;
            // strip comments
            size_t c = line.find("//");
            if (c != string::npos) line.erase(c);
            c = line.find('#');
            if (c != string::npos) line.erase(c);
            // trim whitespace
            size_t b = line.find_first_not_of(" \t\r\n");
            if (b == string::npos) continue;
            size_t e = line.find_last_not_of(" \t\r\n");
            line = line.substr(b, e - b + 1);

            try {
                memory[addr] = (uint32_t)stoul(line, nullptr, 16);
            } catch (...) {
                throw runtime_error("Bad hex on line " + to_string(lineno) + ": " + line);
            }
            addr += 4;
        }
        prog_start = start_addr;
        prog_end = addr;
        pc = start_addr;
        halted = false;
        halt_reason.clear();
    }

    // Read without inserting a phantom entry into the map.
    uint32_t read_mem(uint32_t addr) const {
        auto it = memory.find(addr);
        return it == memory.end() ? 0u : it->second;
    }

    bool mem_exists(uint32_t addr) const { return memory.find(addr) != memory.end(); }

    int32_t sign_extend(uint32_t val, int bits) {
        uint32_t mask = 1u << (bits - 1);
        return (int32_t)((val ^ mask) - mask);
    }

    // Walks the sequencer for one opcode and records (microPC, control signals)
    // per micro-step. Stops when microPC stops advancing, on an exception
    // (e.g. illegal opcode), or after max_steps (safety cap).
    vector<MicroRow> trace_micro(uint8_t opcode, string& err, int max_steps = 8) {
        vector<MicroRow> rows;
        try {
            sequencer.start(opcode);
            for (int i = 0; i < max_steps; ++i) {
                uint16_t upc = sequencer.micro_pc();
                rows.push_back({upc, makeControlPacket(sequencer.current())});
                sequencer.step();
                if (sequencer.micro_pc() == upc) break;
            }
        } catch (const exception& e) {
            err = e.what();
        }
        return rows;
    }

    bool pipeline_empty() const {
        return !if_id.valid && !id_rr.valid && !rr_ex.valid && !ex_ma.valid && !ma_rw.valid;
    }

    void raise_fault(const string& msg) {
        halted = true;
        halt_reason = "EXCEPTION: " + msg;
    }

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

        // ==========================================
        // HAZARD DETECTION UNIT
        // ==========================================
        int32_t forward_val1 = rr_ex.val1;
        int32_t forward_val2 = rr_ex.val2;
        int32_t forward_val3 = rr_ex.val3;

        // Exact forwardable data (handles CALL's PC+4)
        int32_t ex_forward_data = ex_ma.alu_result;
        if (ex_ma.ctrl.write_back == WriteBackControl::PC_PLUS_4) ex_forward_data = ex_ma.pc + 4;

        if (ex_ma.valid && ex_ma.ctrl.register_write && ex_ma.rd != 0) {
            if (ex_ma.rd == rr_ex.rs1) forward_val1 = ex_forward_data;
            if (ex_ma.rd == rr_ex.rs2 && !rr_ex.I) forward_val2 = ex_forward_data;
            if (ex_ma.rd == rr_ex.rd)  forward_val3 = ex_forward_data;
        }
        if (ma_rw.valid && ma_rw.ctrl.register_write && ma_rw.rd != 0) {
            if (ma_rw.rd == rr_ex.rs1 && !(ex_ma.valid && ex_ma.ctrl.register_write && ex_ma.rd == rr_ex.rs1))
                forward_val1 = ma_rw.write_data;
            if (ma_rw.rd == rr_ex.rs2 && !rr_ex.I && !(ex_ma.valid && ex_ma.ctrl.register_write && ex_ma.rd == rr_ex.rs2))
                forward_val2 = ma_rw.write_data;
            if (ma_rw.rd == rr_ex.rd && !(ex_ma.valid && ex_ma.ctrl.register_write && ex_ma.rd == rr_ex.rd))
                forward_val3 = ma_rw.write_data;
        }

        // Load-use stall. Branches/CALL/RET have no real rs1/rs2/rd fields
        // (those bits are offset bits), so they never need to stall here.
        bool stall = false;
        bool id_is_branch = (id_rr.ctrl.branch_control != BranchControl::NONE);
        if (id_rr.valid && rr_ex.valid && rr_ex.ctrl.memory_read && !id_is_branch) {
            if (rr_ex.rd == id_rr.rs1 || (!id_rr.I && rr_ex.rd == id_rr.rs2) || rr_ex.rd == id_rr.rd) {
                stall = true;
            }
        }
        if (stall) { stats.stalls++; stalled_now = true; }

        bool flush = false;

        // ==========================================
        // Stage 6: RW
        // ==========================================
        if (ma_rw.valid) {
            stats.retired++;
            if (ma_rw.opcode == OPC_HALT) {
                halted = true;
                halt_reason = "HALT";
            } else if (ma_rw.ctrl.register_write && ma_rw.rd != 0) {
                regs[ma_rw.rd] = ma_rw.write_data;
            }
        }
        if (halted) return;   // squash everything younger than HALT

        // ==========================================
        // Stage 5: MA
        // ==========================================
        ma_rw.valid = ex_ma.valid;
        if (ex_ma.valid) {
            ma_rw.pc = ex_ma.pc;
            ma_rw.opcode = ex_ma.opcode;
            ma_rw.rd = ex_ma.rd;
            ma_rw.ctrl = ex_ma.ctrl;

            uint32_t addr = (uint32_t)ex_ma.alu_result;
            if ((ex_ma.ctrl.memory_read || ex_ma.ctrl.memory_write) && (addr & 3u)) {
                raise_fault("unaligned memory access at " + hex32(addr) +
                            " (PC " + hex32(ex_ma.pc) + ")");
                ma_rw.valid = false;
                return;
            }

            if (ex_ma.ctrl.memory_write) memory[addr] = (uint32_t)ex_ma.val3;

            if (ex_ma.ctrl.write_back == WriteBackControl::MEMORY_DATA) {
                ma_rw.write_data = (int32_t)read_mem(addr);
            } else if (ex_ma.ctrl.write_back == WriteBackControl::PC_PLUS_4) {
                ma_rw.write_data = ex_ma.pc + 4;
            } else {
                ma_rw.write_data = ex_ma.alu_result;
            }
        }

        // ==========================================
        // Stage 4: EX
        // ==========================================
        ex_ma.valid = rr_ex.valid;
        if (rr_ex.valid) {
            ex_ma.pc = rr_ex.pc;
            ex_ma.opcode = rr_ex.opcode;
            ex_ma.rd = rr_ex.rd;
            ex_ma.ctrl = rr_ex.ctrl;
            ex_ma.val3 = forward_val3;

            AluOp alu_op = AluOp::ADD;
            switch (rr_ex.ctrl.alu_control) {
                case ALUControl::SUB: alu_op = AluOp::SUB; break;
                case ALUControl::MUL: alu_op = AluOp::MUL; break;
                case ALUControl::DIV: alu_op = AluOp::DIV; break;
                case ALUControl::MOD: alu_op = AluOp::MOD; break;
                case ALUControl::AND: alu_op = AluOp::AND; break;
                case ALUControl::OR:  alu_op = AluOp::OR; break;
                case ALUControl::NOT: alu_op = AluOp::NOT; break;
                case ALUControl::MOV: alu_op = AluOp::MOV; break;
                case ALUControl::LSL: alu_op = AluOp::LSL; break;
                case ALUControl::LSR: alu_op = AluOp::LSR; break;
                case ALUControl::ASR: alu_op = AluOp::ASR; break;
                case ALUControl::CMP: alu_op = AluOp::CMP; break;
                default: alu_op = AluOp::ADD; break;
            }

            int32_t op_a = forward_val1;
            int32_t op_b = rr_ex.I ? rr_ex.imm18 : forward_val2;
            if (rr_ex.ctrl.memory_read || rr_ex.ctrl.memory_write) op_b = rr_ex.imm18;

            if ((alu_op == AluOp::DIV || alu_op == AluOp::MOD) && op_b == 0) {
                raise_fault("divide by zero at PC " + hex32(rr_ex.pc));
                ex_ma.valid = false;
                return;
            }

            AluResult res = alu.execute(alu_op, op_a, op_b, flags);
            ex_ma.alu_result = res.value;

            if (rr_ex.ctrl.flag_write) flags = res.flags;

            bool taken = false;
            switch (rr_ex.ctrl.branch_control) {
                case BranchControl::B:
                case BranchControl::CALL:
                    pc = rr_ex.pc + 4 + rr_ex.branch_offset; taken = true; break;
                case BranchControl::BEQ:
                    if (flags.E)  { pc = rr_ex.pc + 4 + rr_ex.branch_offset; taken = true; }
                    break;
                case BranchControl::BGT:
                    if (flags.GT) { pc = rr_ex.pc + 4 + rr_ex.branch_offset; taken = true; }
                    break;
                case BranchControl::RET:
                    pc = (uint32_t)forward_val1; taken = true; break;
                default: break;
            }
            if (taken) flush = true;
        }

        // ==========================================
        // Stage 3: RR
        // ==========================================
        if (stall) {
            rr_ex.valid = false;
        } else {
            rr_ex.valid = id_rr.valid;
            if (id_rr.valid) {
                rr_ex.pc = id_rr.pc;
                rr_ex.opcode = id_rr.opcode;
                rr_ex.rd = id_rr.rd;
                rr_ex.rs1 = id_rr.rs1;
                rr_ex.rs2 = id_rr.rs2;
                rr_ex.I = id_rr.I;
                rr_ex.imm18 = id_rr.imm18;
                rr_ex.branch_offset = id_rr.branch_offset;
                rr_ex.ctrl = id_rr.ctrl;

                rr_ex.val1 = regs[id_rr.rs1];
                rr_ex.val2 = regs[id_rr.rs2];
                rr_ex.val3 = regs[id_rr.rd];
            }
        }

        // ==========================================
        // Stage 2: ID
        // ==========================================
        if (!stall) {
            id_rr.valid = if_id.valid;
            if (if_id.valid) {
                uint32_t instr = if_id.instr;
                id_rr.pc = if_id.pc;
                id_rr.opcode = (instr >> 27) & 0x1F;
                id_rr.I = (instr >> 26) & 0x1;
                id_rr.rd = (instr >> 22) & 0xF;
                id_rr.rs1 = (instr >> 18) & 0xF;

                uint32_t low18 = instr & 0x3FFFF;
                id_rr.rs2 = (low18 >> 14) & 0xF;
                id_rr.imm18 = sign_extend(low18, 18);

                uint32_t offset27 = instr & 0x7FFFFFF;
                id_rr.branch_offset = sign_extend(offset27, 27) * 4;

                try {
                    sequencer.start((uint8_t)id_rr.opcode);
                    id_rr.ctrl = makeControlPacket(sequencer.current());
                    id_rr.micro_pc = sequencer.micro_pc();
                } catch (const exception& e) {
                    raise_fault("illegal opcode 0x" + to_string(id_rr.opcode) + " at PC " +
                                hex32(id_rr.pc) + " (" + e.what() + ")");
                    id_rr.valid = false;
                    return;
                }
                last_opcode = id_rr.opcode;
                have_opcode = true;

                // Fixed link register for CALL (writes it) and RET (reads it)
                if (id_rr.ctrl.branch_control == BranchControl::CALL) id_rr.rd = LINK_REG;
                if (id_rr.ctrl.branch_control == BranchControl::RET)  id_rr.rs1 = LINK_REG;
            }
        }

        // ==========================================
        // Stage 1: IF
        // ==========================================
        if (!stall) {
            // Valid = PC inside the loaded program (a 0x0 word is a legal instruction)
            bool in_prog = (pc >= prog_start && pc < prog_end);
            if_id.valid = in_prog;
            if_id.pc = pc;
            if_id.instr = in_prog ? read_mem(pc) : 0;
            if (in_prog) pc += 4;
        }

        if (flush) {
            rr_ex.valid = false;
            id_rr.valid = false;
            stats.flushes++;
            flushed_now = true;
        }

        // Fell off the end of the program without a HALT: stop cleanly.
        if (pipeline_empty() && (pc < prog_start || pc >= prog_end)) {
            halted = true;
            halt_reason = "end of program reached (no HALT executed)";
        }
    }
};

class Debugger {
    unique_ptr<Simulator> sim;
    set<uint32_t> breakpoints;
    string hex_path;

    static string stage_pc(bool valid, uint32_t pc) {
        return valid ? hex32(pc) : string("   ----   ");
    }

    void print_state() {
        Simulator& s = *sim;
        cout << "\n=== Cycle " << dec << s.stats.cycles << " ===";
        if (s.stalled_now) cout << "  [STALL: load-use hazard]";
        if (s.flushed_now) cout << "  [FLUSH: branch taken]";
        cout << "\n";

        cout << "Stage | PC         | Detail\n";
        cout << "------+------------+--------------------------------\n";
        cout << "IF/ID | " << stage_pc(s.if_id.valid, s.if_id.pc)
             << " | instr " << (s.if_id.valid ? hex32(s.if_id.instr) : "-") << "\n";
        cout << "ID/RR | " << stage_pc(s.id_rr.valid, s.id_rr.pc)
             << " | opcode 0x" << hex << (s.id_rr.valid ? s.id_rr.opcode : 0) << dec
             << "  rd R" << (s.id_rr.valid ? s.id_rr.rd : 0)
             << "  rs1 R" << (s.id_rr.valid ? s.id_rr.rs1 : 0)
             << "  rs2 R" << (s.id_rr.valid ? s.id_rr.rs2 : 0)
             << "  I=" << (s.id_rr.valid ? s.id_rr.I : 0)
             << "  uPC=0x" << hex << (s.id_rr.valid ? s.id_rr.micro_pc : 0) << dec << "\n";
        cout << "RR/EX | " << stage_pc(s.rr_ex.valid, s.rr_ex.pc)
             << " | val1 " << hex32(s.rr_ex.valid ? (uint32_t)s.rr_ex.val1 : 0)
             << "  val2 " << hex32(s.rr_ex.valid ? (uint32_t)s.rr_ex.val2 : 0) << "\n";
        cout << "EX/MA | " << stage_pc(s.ex_ma.valid, s.ex_ma.pc)
             << " | ALU result " << hex32(s.ex_ma.valid ? (uint32_t)s.ex_ma.alu_result : 0) << "\n";
        cout << "MA/RW | " << stage_pc(s.ma_rw.valid, s.ma_rw.pc)
             << " | write data " << hex32(s.ma_rw.valid ? (uint32_t)s.ma_rw.write_data : 0) << "\n";

        cout << "\nPC: " << hex32(s.pc)
             << "   Flags: E=" << s.flags.E << " GT=" << s.flags.GT << "\n";
        for (int i = 0; i < 16; i += 4) {
            for (int j = 0; j < 4; ++j) {
                cout << "R" << dec << left << setw(2) << (i + j) << right << "=" << hex32(s.regs[i + j]) << "  ";
            }
            cout << "\n";
        }
        cout << "=====================\n";
        if (s.halted) {
            cout << "Program stopped: " << s.halt_reason << "\n";
            cout << "\n=== Execution Summary ===\n";
            cout << "Total Cycles:    " << s.stats.cycles << "\n";
            cout << "Instructions:    " << s.stats.retired << "\n";
            cout << "Branch Flushes:  " << s.stats.flushes << "\n";
            cout << "Load-Use Stalls: " << s.stats.stalls << "\n";
            cout << "=========================\n";
        }
    }

    static void print_ctrl(const ControlPacket& c) {
        cout << "mem_rd=" << static_cast<int>(c.memory_read)
             << " mem_wr=" << static_cast<int>(c.memory_write)
             << " reg_wr=" << static_cast<int>(c.register_write)
             << " flag_wr=" << static_cast<int>(c.flag_write)
             << " alu=" << static_cast<int>(c.alu_control)
             << " branch=" << static_cast<int>(c.branch_control)
             << " wb=" << static_cast<int>(c.write_back);
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
        else if (rows.empty()) cout << "(no microinstructions)\n";
    }

    void load_fresh() {
        sim = make_unique<Simulator>();
        sim->load_memory_from_hex(hex_path);
    }

    // Run up to n cycles (n==0: until halt/breakpoint). Returns true if stopped on breakpoint.
    bool run_cycles(uint64_t n) {
        uint64_t done = 0;
        bool first = true;
        while (!sim->halted && (n == 0 || done < n)) {
            // Breakpoint: instruction at this address is about to execute (in EX).
            if (!first && sim->rr_ex.valid && breakpoints.count(sim->rr_ex.pc)) {
                cout << "Breakpoint hit at " << hex32(sim->rr_ex.pc) << " (about to execute)\n";
                return true;
            }
            sim->step();
            ++done;
            first = false;
        }
        return false;
    }

public:
    void attach(const string& hex_filepath) {
        try {
            hex_path = hex_filepath;
            load_fresh();
            cout << "Loaded program from " << hex_filepath << " ("
                 << (sim->prog_end - sim->prog_start) / 4 << " words)\n";
            repl();
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
                     << "  print <Rn|hex_addr> (p)  show register or memory word\n"
                     << "  mem <hex_addr> [N] (x)   dump N words (default 8)\n"
                     << "  micro [hex_opcode] (m)   microPC trace (default: last decoded opcode)\n"
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
                        cout << "Breakpoint set at " << hex32(addr) << "\n";
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
                            if (breakpoints.erase(addr)) cout << "Removed " << hex32(addr) << "\n";
                            else cout << "No breakpoint at " << hex32(addr) << "\n";
                        } catch (...) { cout << "Invalid address.\n"; }
                    }
                } else {
                    cout << "Usage: delete <hex_addr|all>\n";
                }
            } else if (cmd == "info" || cmd == "i") {
                if (breakpoints.empty()) cout << "No breakpoints.\n";
                else for (uint32_t b : breakpoints) cout << "  " << hex32(b) << "\n";
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
                            if (reg >= 0 && reg < 16) cout << target << " = " << hex32(sim->regs[reg]) << "\n";
                            else cout << "Register must be R0-R15.\n";
                        } else {
                            uint32_t addr = (uint32_t)stoul(target, nullptr, 16);
                            cout << "Mem[" << hex32(addr) << "] = " << hex32(sim->read_mem(addr))
                                 << (sim->mem_exists(addr) ? "" : "  (uninitialised)") << "\n";
                        }
                    } catch (...) { cout << "Invalid target format.\n"; }
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
                        cout << hex32(a) << ": " << hex32(sim->read_mem(a)) << "\n";
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

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: risc201sim <program.hex>\n";
        return 1;
    }

    risc201::Debugger dbg;
    dbg.attach(argv[1]);

    return 0;
}