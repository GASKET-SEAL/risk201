#include "demos.hpp"

#include <iomanip>
#include <iostream>

#include "common.hpp"
#include "macro_expander.hpp"
#include "stack_guard.hpp"
#include "vm.hpp"

namespace risc201 {

bool runSource(const std::string& name, const std::string& src, const Options& o,
               std::optional<Fault> expect, const std::map<int, uint32_t>& regs) {
    StackGuard guard(STACK_BASE, STACK_TOP, o.shadow, o.canary);
    VM vm(guard, std::cout);
    MacroExpander mx(!o.naive);
    std::optional<Fault> got;
    std::cout << "\n=== " << name << " ===\n";
    try {
        auto lines = mx.expand(src);
        if (o.verbose) {
            std::cout << "--- expanded code (original line -> instruction) ---\n";
            for (auto& l : lines) std::cout << std::setw(4) << l.srcLine << " | " << l.text << "\n";
        }
        const auto& st = mx.stats();
        if (st.naiveInstr)
            std::cout << "macro stats: " << st.emittedInstr << " instr emitted vs " << st.naiveInstr
                      << " naive, " << st.emittedSpUpdates << " SP update(s) vs "
                      << st.naiveSpUpdates << " naive, " << st.batches << " batch(es)\n";
        vm.load(lines);
        vm.run();
        std::cout << "program halted normally\n";
    } catch (StackException& e) {
        got = e.fault;
        std::cout << "EXCEPTION " << faultName(e.fault) << ": " << e.what() << "\n";
        if (!e.srcText.empty())
            std::cout << "  at source line " << e.srcLine << ", instruction #" << e.pc
                      << " `" << trim(e.srcText) << "`\n";
        else if (e.srcLine >= 0)
            std::cout << "  at source line " << e.srcLine << "\n";
        if (o.verbose && e.fault != Fault::MacroError) vm.dumpStack();
    }
    bool ok = (got == expect);
    if (ok && !got)
        for (auto& [r, v] : regs)
            if (vm.reg(r) != v) {
                std::cout << "r" << r << " = " << vm.reg(r) << ", expected " << v << "\n";
                ok = false;
            }
    return ok;
}

std::vector<Demo> demos(const Options& o) {
    std::vector<Demo> d;
    d.push_back({"1. push/pop round trip + call (batched macros, viz inside callee)", R"(
        mov r1, 11
        mov r2, 22
        mov r3, 33
        push r1, r2, r3        ; one batch
        mov r1, 0
        mov r2, 0
        mov r3, 0
        pop r3, r2, r1         ; LIFO: reverse order restores the registers
        call fn
        halt
    fn:
        push r1                ; three separate lines still merge
        push r2
        push r3
        mov r1, 7
        viz
        pop r3
        pop r2
        pop r1
        ret
    )", std::nullopt, {{1, 11}, {2, 22}, {3, 33}}});

    d.push_back({"2. nested calls (two frames on the visualiser)", R"(
        mov r1, 1
        call outer
        halt
    outer:
        push r1
        mov r1, 2
        call inner
        pop r1
        ret
    inner:
        push r1, r2
        viz
        pop r2, r1
        ret
    )", std::nullopt, {{1, 1}}});

    // Demo 3: linear overflow one word past char buf[8].
    // Canary on : 8[r13] hits the canary            -> CanaryCorrupted
    // Canary off: 8[r13] hits the return address    -> ShadowStackMismatch (shadow on)
    std::optional<Fault> exp3 = o.canary ? std::optional<Fault>(Fault::CanaryCorrupted)
                              : (o.shadow ? std::optional<Fault>(Fault::ShadowStackMismatch)
                                          : std::optional<Fault>(Fault::BadInstruction));
    d.push_back({"3. linear buffer overflow reaches canary", R"(
        call victim
        halt
    victim:
        sub r13, r13, 8        ; char buf[8]
        mov r1, 255
        st r1, 0[r13]
        st r1, 4[r13]
        st r1, 8[r13]          ; one word too far -> canary (or retSlot when --no-canary)
        add r13, r13, 8
        ret
    )", exp3, {}});

    // Demo 4: direct overwrite of the saved return address, skipping the canary.
    // Return address is at 12[r13] with canary on, 8[r13] with --no-canary.
    std::string retOff = o.canary ? "12" : "8";
    std::optional<Fault> exp4 = o.shadow ? std::optional<Fault>(Fault::ShadowStackMismatch)
                                         : std::nullopt;
    d.push_back({"4. return address overwritten, canary skipped -> shadow stack",
        "        call victim\n"
        "        halt\n"
        "    victim:\n"
        "        sub r13, r13, 8\n"
        "        mov r1, 7\n"
        "        st r1, " + retOff + "[r13]         ; direct write to the saved return address\n"
        "        add r13, r13, 8\n"
        "        ret\n"
        "        halt\n",
        exp4, {}});

    d.push_back({"5. pop on empty stack", "pop r1\nhalt\n", Fault::StackUnderflow, {}});
    d.push_back({"6. runaway allocation",
                 "sub r13, r13, 2000\nsub r13, r13, 2000\nsub r13, r13, 2000\nhalt\n",
                 Fault::StackOverflow, {}});
    d.push_back({"7. misaligned SP", "sub r13, r13, 3\nhalt\n", Fault::MisalignedSP, {}});
    d.push_back({"8. access below SP (freed memory)",
                 "sub r13, r13, 8\nadd r13, r13, 8\nst r1, -4[r13]\nhalt\n", Fault::BelowSP, {}});
    d.push_back({"9. wild pointer", "mov r2, 0x20000\nld r1, 0[r2]\nhalt\n", Fault::OutOfBounds, {}});
    d.push_back({"10. ret without call", "ret\n", Fault::ShadowStackEmpty, {}});
    d.push_back({"11. unbalanced frame at ret",
                 "call f\nhalt\nf:\nsub r13, r13, 4\nret\n", Fault::UnbalancedFrame, {}});
    d.push_back({"12. macro validation: push r13", "push r1, r13\n", Fault::MacroError, {}});
    d.push_back({"13. macro validation: bad register", "pop r1, r99\n", Fault::MacroError, {}});
    d.push_back({"14. macro validation: duplicate reg in pop", "pop r1, r1\n", Fault::MacroError, {}});
    d.push_back({"15. stack pointer alias overflow via general register",
                 "mov r2, r13\nsub r2, r2, 2044\nsub r2, r2, 2044\nld r1, -12[r2]\nhalt\n",
                 Fault::StackOverflow, {}});
    d.push_back({"16. pop into own frame header (canary/ret)",
                 "call f\nhalt\nf:\npop r1\nret\n", Fault::UnbalancedFrame, {}});
    d.push_back({"17. pointer difference is not a stack pointer",
                 "sub r2, r13, r13\nst r1, 0[r2]\nhalt\n", std::nullopt, {}});
    d.push_back({"18. duplicate label", "a:\na:\nhalt\n", Fault::BadInstruction, {}});
    return d;
}

}  // namespace risc201
