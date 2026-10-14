#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cctype>

namespace risc201 {

// ==========================================
// 1. UTILITIES (Self-Contained for Compilation)
// ==========================================
namespace util {
    inline std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    inline std::string stripComment(const std::string& s) {
        size_t p = s.find_first_of(";#");
        return p == std::string::npos ? s : s.substr(0, p);
    }

    inline std::string toLower(const std::string& s) {
        std::string r = s;
        for (auto& c : r) c = (char)tolower((unsigned char)c);
        return r;
    }
}

// ==========================================
// 2. MEMORY CONTROLLER & EXCEPTIONS
// ==========================================
class StackCorruptionException : public std::runtime_error {
public:
    explicit StackCorruptionException(const std::string& msg) : std::runtime_error("Stack Exception: " + msg) {}
};

class MemoryController {
private:
    std::vector<uint8_t> memory;
    uint32_t stackBase;
    uint32_t stackLimit;
    uint32_t maxSpReached; // Tracks deepest stack penetration (Dirty Watermark)

public:
    MemoryController(size_t memSize, uint32_t sBase, uint32_t sLimit) 
        : memory(memSize, 0), stackBase(sBase), stackLimit(sLimit), maxSpReached(sBase) {}

    void verifyStackAccess(uint32_t address) {
        if (address < stackLimit) {
            std::stringstream ss;
            ss << "Stack Underflow (SP exceeded allocation limit of 0x" << std::hex << stackLimit << ")";
            throw StackCorruptionException(ss.str());
        }
        if (address > stackBase) {
            std::stringstream ss;
            ss << "Stack Overflow (SP corrupted above base frame of 0x" << std::hex << stackBase << ")";
            throw StackCorruptionException(ss.str());
        }
        maxSpReached = std::min(maxSpReached, address);
    }

    void writeWord(uint32_t address, uint32_t value) {
        if (address >= stackLimit && address <= stackBase) {
            verifyStackAccess(address);
        }
        memory[address] = value & 0xFF;
        memory[address + 1] = (value >> 8) & 0xFF;
        memory[address + 2] = (value >> 16) & 0xFF;
        memory[address + 3] = (value >> 24) & 0xFF;
    }

    uint32_t readWord(uint32_t address) {
        if (address >= stackLimit && address <= stackBase) {
            verifyStackAccess(address);
        }
        return memory[address] | (memory[address + 1] << 8) | 
               (memory[address + 2] << 16) | (memory[address + 3] << 24);
    }

    uint32_t getStackBase() const { return stackBase; }
    uint32_t getStackLimit() const { return stackLimit; }
    uint32_t getMaxSpReached() const { return maxSpReached; }
};

// ==========================================
// 3. STACK PREPROCESSOR (Macro Batching)
// ==========================================
class StackPreprocessor {
public:
    static std::vector<std::string> expandMacros(const std::vector<std::string>& sourceLines) {
        std::vector<std::string> expanded;
        std::vector<std::string> pushBatch;
        std::vector<std::string> popBatch;

        auto flushPushes = [&]() {
            if (pushBatch.empty()) return;
            int batchSize = pushBatch.size();
            int totalOffset = batchSize * 4;
            
            expanded.push_back("    sub r13, r13, " + std::to_string(totalOffset) + "    ; macro-expanded push batch");
            for (int i = 0; i < batchSize; ++i) {
                int offset = (batchSize - 1 - i) * 4;
                expanded.push_back("    st " + pushBatch[i] + ", " + std::to_string(offset) + "[r13]");
            }
            pushBatch.clear();
        };

        auto flushPops = [&]() {
            if (popBatch.empty()) return;
            int batchSize = popBatch.size();
            int totalOffset = batchSize * 4;
            
            expanded.push_back("    ; macro-expanded pop batch");
            for (int i = 0; i < batchSize; ++i) {
                int offset = i * 4;
                expanded.push_back("    ld " + popBatch[i] + ", " + std::to_string(offset) + "[r13]");
            }
            expanded.push_back("    add r13, r13, " + std::to_string(totalOffset));
            popBatch.clear();
        };

        for (const auto& line : sourceLines) {
            std::string trimmed = util::trim(util::stripComment(line));
            if (trimmed.empty()) {
                expanded.push_back(line);
                continue;
            }

            std::istringstream iss(trimmed);
            std::string mnemonic, operand;
            iss >> mnemonic >> operand;
            mnemonic = util::toLower(mnemonic);

            if (mnemonic == "push") {
                flushPops(); 
                pushBatch.push_back(operand);
            } else if (mnemonic == "pop") {
                flushPushes(); 
                popBatch.push_back(operand);
            } else {
                flushPushes();
                flushPops();
                expanded.push_back(line);
            }
        }
        
        flushPushes();
        flushPops();
        
        return expanded;
    }
};

// ==========================================
// 4. CPU SIMULATOR (Shadow Stack Integration)
// ==========================================
class CpuSimulator {
private:
    MemoryController& mem;
    std::vector<uint32_t> shadowStack;
    uint32_t pc;
    
public:
    CpuSimulator(MemoryController& memory) : mem(memory), pc(0x1000) {}

    uint32_t getPC() const { return pc; }

    void executeCall(uint32_t targetAddress, uint32_t& currentSP) {
        uint32_t returnAddress = pc + 4;
        
        currentSP -= 4;
        mem.writeWord(currentSP, returnAddress);
        
        shadowStack.push_back(returnAddress);
        pc = targetAddress;
    }

    void executeRet(uint32_t& currentSP) {
        if (shadowStack.empty()) {
            throw StackCorruptionException("Shadow Stack Underflow: 'ret' called without matching 'call'");
        }

        uint32_t physicalRetAddr = mem.readWord(currentSP);
        currentSP += 4;

        uint32_t shadowRetAddr = shadowStack.back();
        shadowStack.pop_back();

        if (physicalRetAddr != shadowRetAddr) {
            std::stringstream ss;
            ss << "Control Flow Hijack Detected! Physical stack return address (0x" 
               << std::hex << physicalRetAddr << ") does not match Shadow Stack (0x" 
               << shadowRetAddr << ").";
            throw StackCorruptionException(ss.str());
        }

        pc = physicalRetAddr;
    }
};

// ==========================================
// 5. STACK VISUALIZER (Dirty-Block Renderer)
// ==========================================
class StackVisualizer {
public:
    static void renderStackFrame(MemoryController& mem, uint32_t currentSP) {
        uint32_t stackBase = mem.getStackBase();
        uint32_t renderLimit = mem.getMaxSpReached();
        if (renderLimit >= 16) renderLimit -= 16; 
        
        std::cout << "\n=== ACTIVE STACK MEMORY FRAME ===\n";
        std::cout << "  Address      Value\n";
        std::cout << "+------------+------------+\n";

        uint32_t alignedSP = currentSP & ~0x3;
        
        for (uint32_t addr = stackBase; addr >= renderLimit; addr -= 4) {
            uint32_t val = mem.readWord(addr);
            
            std::cout << "| 0x" << std::setfill('0') << std::setw(8) << std::hex << addr << " | ";
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << val << " |";

            if (addr == stackBase) std::cout << " <-- BASE";
            else if (addr == alignedSP) std::cout << " <-- SP (Current)";
            else if (addr < alignedSP && addr >= mem.getMaxSpReached()) std::cout << " (Dirty/Abandoned)";
            
            std::cout << "\n";
            if (addr < 4) break; 
        }
        std::cout << "+------------+------------+\n";
        std::cout << "Shadow Stack Depth: Secured\n\n";
        std::cout << std::dec; 
    }
};

} // namespace risc201

// ==========================================
// 6. MAIN TEST EXECUTION
// ==========================================
using namespace risc201;

int main() {
    std::cout << "--- Testing RISC201 Stack Architecture ---\n";

    // 1. Test Preprocessor Macro Batching
    std::vector<std::string> source = {
        "push r1",
        "push r2",
        "push r3",
        "nop",
        "pop r3",
        "pop r2",
        "pop r1"
    };

    std::cout << "\n[1] Preprocessor Output:\n";
    auto expanded = StackPreprocessor::expandMacros(source);
    for (const auto& line : expanded) {
        std::cout << line << "\n";
    }

    // 2. Initialize Hardware Simulation
    const size_t MEM_SIZE = 0x20000;
    const uint32_t STACK_BASE = 0x1FFFF; // Aligned to 0x1FFFC internally
    const uint32_t STACK_LIMIT = 0x1F000;
    
    MemoryController mem(MEM_SIZE, STACK_BASE & ~0x3, STACK_LIMIT);
    CpuSimulator cpu(mem);
    uint32_t r13_sp = STACK_BASE & ~0x3; // SP initialized to base

    try {
        std::cout << "\n[2] Executing Subroutine CALL (Target: 0x4000)...\n";
        cpu.executeCall(0x4000, r13_sp);
        
        std::cout << "    Pushing local variables to stack...\n";
        r13_sp -= 12; // Simulate batch push of 3 registers
        mem.writeWord(r13_sp + 8, 0xAAAAAAAA);
        mem.writeWord(r13_sp + 4, 0xBBBBBBBB);
        mem.writeWord(r13_sp, 0xCCCCCCCC);

        StackVisualizer::renderStackFrame(mem, r13_sp);

        std::cout << "[3] Popping local variables...\n";
        r13_sp += 12; // Simulate batch pop

        StackVisualizer::renderStackFrame(mem, r13_sp);

        std::cout << "[4] Executing RET...\n";
        cpu.executeRet(r13_sp);
        std::cout << "    RET successful. PC restored to: 0x" << std::hex << cpu.getPC() << std::dec << "\n";

        // 3. Test Security Exceptions (Buffer Overflow Simulation)
        std::cout << "\n[5] Simulating Control Flow Hijack (Buffer Overflow)...\n";
        cpu.executeCall(0x5000, r13_sp);
        
        // Malicious actor overwrites the return address on the physical stack
        mem.writeWord(r13_sp, 0x99999999); 
        
        std::cout << "    Executing tampered RET...\n";
        cpu.executeRet(r13_sp); 

    } catch (const StackCorruptionException& e) {
        std::cerr << "\n[SECURITY EXCEPTION CAUGHT]\n" << e.what() << "\n";
    }

    return 0;
}