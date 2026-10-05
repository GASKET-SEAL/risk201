#include "../include/disassembler.hpp"
#include "../include/decoder.hpp"
#include "../include/opcode_table.hpp"
#include "../include/reader.hpp"
#include <cstdio>
#include <set>
#include <sstream>

namespace risc201 {

static std::string labelFor(uint32_t addr, const std::map<uint32_t, std::string>& symbols) {
    auto it = symbols.find(addr);
    if (it != symbols.end()) return it->second;
    char buf[16];
    snprintf(buf, sizeof(buf), "L%08x", addr);
    return buf;
}

std::string Disassembler::disassembleOne(uint32_t word, uint32_t address,
                                          const std::map<uint32_t, std::string>& symbols) {
    OpcodeTable opcodeTable;
    Decoder decoder(opcodeTable);
    DecodedInstruction d = decoder.decode(word, address);

    std::ostringstream out;
    out << d.mnemonic;
    if (d.isBranchLike) {
        out << " " << labelFor(d.branchTargetAddress, symbols);
    } else if (!d.operandText.empty()) {
        out << " " << d.operandText;
    }
    return out.str();
}

std::string Disassembler::disassembleWords(const std::vector<uint32_t>& words,
                                            const std::map<uint32_t, std::string>& symbols) {
    uint32_t programEnd = (uint32_t)words.size() * 4;

    OpcodeTable opcodeTable;
    Decoder decoder(opcodeTable);

    std::vector<DecodedInstruction> instrs;
    std::set<uint32_t> targets;

    for (size_t i = 0; i < words.size(); i++) {
        uint32_t addr = (uint32_t)(i * 4);
        DecodedInstruction d = decoder.decode(words[i], addr);
        if (d.isBranchLike && d.branchTargetAddress < programEnd && d.branchTargetAddress % 4 == 0) {
            d.branchTargetInRange = true;
            targets.insert(d.branchTargetAddress);
        }
        instrs.push_back(d);
    }

    for (const auto& [addr, name] : symbols) targets.insert(addr);

    std::ostringstream out;
    for (const DecodedInstruction& d : instrs) {
        if (targets.count(d.address)) {
            out << labelFor(d.address, symbols) << ":\n";
        }

        char addrBuf[16];
        snprintf(addrBuf, sizeof(addrBuf), "%08x", d.address);
        out << "    " << d.mnemonic;

        if (d.isBranchLike) {
            std::string tgt = d.branchTargetInRange ? labelFor(d.branchTargetAddress, symbols)
                                                      : std::to_string(d.branchTargetAddress);
            out << " " << tgt;
        } else if (!d.operandText.empty()) {
            out << " " << d.operandText;
        }

        char wbuf[9];
        snprintf(wbuf, sizeof(wbuf), "%08x", d.word);
        out << "    ; " << addrBuf << ": " << wbuf << "\n";
    }

    return out.str();
}

std::string Disassembler::disassemble(const std::string& inputPath) {
    std::vector<uint32_t> words = Reader::readWords(inputPath);
    return disassembleWords(words);
}

}