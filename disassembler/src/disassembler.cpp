#include "../include/disassembler.hpp"
#include "../include/decoder.hpp"
#include "../include/opcode_table.hpp"
#include "../include/reader.hpp"
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace risc201 {

static std::string labelName(uint32_t addr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "L%08x", addr);
    return buf;
}

std::string Disassembler::disassemble(const std::string& inputPath) {
    std::vector<uint32_t> words = Reader::readWords(inputPath);
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

    std::ostringstream out;
    for (const DecodedInstruction& d : instrs) {
        if (targets.count(d.address)) {
            out << labelName(d.address) << ":\n";
        }

        char addrBuf[16];
        snprintf(addrBuf, sizeof(addrBuf), "%08x", d.address);
        out << "    " << d.mnemonic;

        if (d.isBranchLike) {
            std::string tgt = d.branchTargetInRange ? labelName(d.branchTargetAddress)
                                                      : std::to_string(d.branchTargetAddress);
            out << " " << tgt;
        } else if (!d.operandText.empty()) {
            out << " " << d.operandText;
        }

        out << "    ; " << addrBuf << ": " << [&]{ char b[9]; snprintf(b, sizeof(b), "%08x", d.word); return std::string(b); }() << "\n";
    }

    return out.str();
}

}
