# RISC201 Processor Design

A complete software implementation of **RISC201**, a 32-bit RISC instruction set architecture, built for **CSC-201 (Computer Organization & Architecture)**. The project is a toolchain plus a cycle-accurate machine model:

| Component | What it is |
|---|---|
| **Assembler** | Two-pass assembler: `.asm` → `.hex` / `.bin` / `.sym` |
| **Disassembler** | `.hex` / `.bin` → readable assembly, with optional symbol names |
| **ALU** | Gate-level-style ALU built from selectable adder, multiplier, divider and barrel-shifter algorithms, verified against a reference ALU |
| **Control unit** | Microprogrammed control store plus every pipeline stage (IF, ID, RR, EX, MA, RW) and a fused 4-stage variant |
| **Stack subsystem** | `push`/`pop` macro expander with batching, a stack guard (bounds, canary, shadow stack), a fault framework and an ASCII stack visualiser |
| **Simulator** | Cycle-accurate **6-stage and 4-stage pipeline simulators** with forwarding, hazard handling, statistics and an **interactive debugger** |

Everything is plain C++17 with no third-party dependencies (the optional test driver is Python 3).

> Upstream: <https://github.com/GASKET-SEAL/risk201> · Branches: `main`, `assembler`, `dhruvi-microprogrammed-control`

---

## Table of contents

1. [Quick start](#1-quick-start)
2. [Repository layout](#2-repository-layout)
3. [The RISC201 ISA](#3-the-risc201-isa)
4. [Assembly language reference](#4-assembly-language-reference)
5. [Assembler](#5-assembler)
6. [Disassembler](#6-disassembler)
7. [ALU](#7-alu)
8. [Control unit and pipeline stages](#8-control-unit-and-pipeline-stages)
9. [Stack subsystem](#9-stack-subsystem)
10. [Simulator and debugger](#10-simulator-and-debugger)
11. [Test programs and expected results](#11-test-programs-and-expected-results)
12. [Testing everything](#12-testing-everything)
13. [Known issues, quirks and stale artefacts](#13-known-issues-quirks-and-stale-artefacts)
14. [Project history and contributors](#14-project-history-and-contributors)

---

## 1. Quick start

### Prerequisites

* A C++17 compiler (`g++` is what every Makefile uses), and `make`
* Python 3 (only for `Simulator/tests/stack_tests.py`)
* Developed on Windows with MinGW-w64 GCC 14.2 (see `.vscode/c_cpp_properties.json`). The same sources build and pass all tests on Linux with `g++`.

### Build

Each tool builds from its own directory. The simulator is the only target that compiles other modules' sources, so it needs the whole tree.

```sh
# Assembler / disassembler
make -C assembler          # -> assembler/risc201asm
make -C disassembler       # -> disassembler/risc201disasm

# ALU library tests
make -C alu                # -> alu/risc201alu and alu/difftest
make -C alu test           # builds and runs both

# Simulator + debugger (needs alu/, control/, stack/, assembler/ sources)
make -C Simulator          # -> Simulator/risc201sim

# Stack subsystem standalone harness
# (stack/Makefile uses Windows cmd.exe syntax, so on Linux/macOS build by hand)
cd stack && g++ -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude -o risc201_stack src/*.cpp
```

### Hello, RISC201

```sh
# 1. Assemble the recursive factorial example
./assembler/risc201asm assembler/tests/factorial.asm -o /tmp/fact
#    assembled 19 instruction(s) -> /tmp/fact.hex, /tmp/fact.bin, /tmp/fact.sym

# 2. Disassemble it again (labels restored from the .sym file)
./disassembler/risc201disasm /tmp/fact.hex -s /tmp/fact.sym

# 3. Run it on the 6-stage pipeline and print a one-line summary
./Simulator/risc201sim --batch Simulator/tests/factorial5.asm
#    pipeline=6 alu=KoggeStone/Booth4/NonRestoring cycles=100 instructions=60 cpi=1.667
#    flushes=15 stalls=0 stop="HALT" r0=5 r1=120 ... r13=65536 ...

# 4. Or step through it interactively
./Simulator/risc201sim Simulator/tests/factorial5.asm
#    (dbg) step 3 / break 0x14 / run / regs / stack / micro / quit
```

---

## 2. Repository layout

```
risk201/
├── README.md
├── .vscode/c_cpp_properties.json      IntelliSense config (hard-coded Windows compiler path)
│
├── alu/                               ── ALU library (namespace risc201)
│   ├── Makefile                       targets: all, risc201alu, difftest, test, clean
│   ├── include/alu.hpp                Alu class, AluOp, AluConfig, flag/result structs
│   ├── include/units.hpp              adder / multiplier / divider / shifter API
│   ├── src/alu.cpp                    op dispatch: structural path + reference path
│   ├── src/units.cpp                  the arithmetic algorithms themselves
│   ├── src/main.cpp                   30 directed unit checks  -> risc201alu
│   └── tests/difftest.cpp             5.8 M-check randomized differential test
│
├── assembler/
│   ├── Makefile                       builds risc201asm from src/*.cpp
│   ├── risc201asm.exe                 prebuilt Windows binary (committed)
│   ├── include/ + src/                lexer, parser, symbol_table, opcode_table,
│   │                                  assembler_pass1/2, diagnostics, utilities,
│   │                                  assembler (driver), main (CLI)
│   └── tests/                         factorial.asm/.hex/.bin/.sym and two
│                                      *_reconstructed_*.asm disassembly round-trip files
│
├── disassembler/
│   ├── Makefile                       builds risc201disasm from src/*.cpp
│   ├── risc201disasm.exe              prebuilt Windows binary (committed)
│   └── include/ + src/                reader, opcode_table, decoder, disassembler, main
│
├── control/                           ── microprogrammed control + pipeline stages
│   ├── include/ + src/                control_memory, microinstruction, microprogram_sequencer,
│   │                                  control_packet, pipeline_registers, register_file,
│   │                                  instruction_memory, data_memory,
│   │                                  instruction_fetch/decode/register_read/execute/
│   │                                  memory_access/write_back _stage, four_stage_components
│   └── tests/                         7 small test programs (no Makefile; see §12)
│
├── stack/                             ── stack subsystem + standalone VM harness
│   ├── Makefile                       Windows-only (cmd.exe `if not exist`, `.\` paths)
│   ├── .gitattributes                 `* text=auto`
│   ├── risc201_stack.exe, build/*.o   committed Windows binary + object files
│   ├── include/ + src/                common, fault, macro_expander, stack_guard,
│   │                                  stack_visualizer  (used by the simulator)
│   │                                  vm, demos, main   (standalone harness only)
│
└── Simulator/                         ── pipeline simulator + debugger
    ├── Makefile                       builds risc201sim from simulator.cpp + all modules
    ├── simulator.cpp                  the whole driver, ~1070 lines
    ├── risc201sim.exe                 prebuilt Windows binary (committed)
    └── tests/
        ├── *.asm / *.hex / *.sym      7 test programs + 1 raw-hex illegal-opcode program
        ├── run_tests.sh               runs every *.asm on both pipelines
        ├── stack_tests.py             runs stack/*.s through the simulator (23 checks)
        └── stack/01..18_*.s           18 stack/fault test programs
```

Roughly 5,000 lines of C++ outside the tests. The simulator is the integration point: it links the ALU, the control-unit stages, the stack subsystem **and** the assembler, so it can execute `.asm` source directly (expand macros → assemble → run).

---

## 3. The RISC201 ISA

### 3.1 Programmer-visible state

| Item | Description |
|---|---|
| **Registers** | 16 general-purpose 32-bit registers `r0`–`r15`. `r0` is an ordinary register (not hard-wired to zero). All registers reset to 0 except `r13`. |
| **`r13`** | Stack pointer **by convention** (the stack subsystem treats it as SP). The simulator initialises it to `0x00010000` (empty full-descending stack). |
| **`r15`** | Link register. `call` writes the return address (PC+4) into it. In `--no-stack` mode `ret` jumps to it. |
| **`r14`** | No special role in the current ISA. (Older sample code used `r14` as SP; see §13.) |
| **PC** | Byte address of the next instruction; always a multiple of 4. Starts at 0. |
| **Flags** | Two architectural flags: **E** (equal) and **GT** (signed greater-than). Written only by `cmp`, read only by `beq` / `bgt`. Reset to 0. |
| **Instruction memory** | Separate from data memory (Harvard organisation). Instructions are 32-bit words at byte addresses 0, 4, 8, … Program loads at address 0. |
| **Data memory** | 64 KiB (`0x0000`–`0xFFFF`), byte-addressed, **word (4-byte) aligned accesses only**. Initialised to 0. Instruction fetch and data access never share storage, so there is no self-modifying code. |
| **Stack region** | The top 4 KiB of data memory, `0xF000`–`0xFFFF`. Full-descending: push = decrement SP then store. See §9. |

Only word accesses exist, so byte order is not observable by programs.

### 3.2 Instruction encoding

Every instruction is exactly 32 bits. Bit 31 is the most significant.

```
bit:  31      27 26 25    22 21    18 17    14 13              0
      +---------+--+--------+--------+--------+----------------+
 reg  | opcode  |I=0|  rd   |  rs1   |  rs2   |    (unused)    |   register form
      +---------+--+--------+--------+--------+----------------+
 imm  | opcode  |I=1|  rd   |  rs1   |     imm18 (signed)      |   immediate form
      +---------+--+--------+--------+-------------------------+
 br   | opcode  |           offset27 (signed, in words)         |   branch form
      +---------+-----------------------------------------------+
 none | opcode  |                  (zero)                       |   nop / ret / halt
      +---------+-----------------------------------------------+
```

| Field | Bits | Notes |
|---|---|---|
| `opcode` | 31:27 | 5 bits; opcodes 0–21 are defined, 22–31 are **illegal** |
| `I` | 26 | 1 = second operand is the sign-extended 18-bit immediate; 0 = second operand is register `rs2`. The assembler sets it automatically. |
| `rd` | 25:22 | destination register, or the **source** register for `st`. For `call` the hardware forces `rd = r15`. |
| `rs1` | 21:18 | first source / base register. For `ret` the hardware forces `rs1 = r15`. |
| `rs2` | 17:14 | second source register (register form) |
| `imm18` | 17:0 | signed, range **−131072 … +131071**. Overlaps `rs2` and the unused bits. |
| `offset27` | 26:0 | branch displacement in **words**, signed, range ±2²⁶ words. Overlaps the `I` bit (ignored for branches). |

Branch target = `(address of the branch + 4) + offset27 × 4`. Offsets are therefore relative to the **next** instruction. Note that `0x00000000` decodes as `add r0, r0, r0`.

### 3.3 Instruction set

| Opc | Binary | Mnemonic | Operands | Operation | Writes |
|---:|:---:|---|---|---|---|
| 0 | `00000` | `add` | `rd, rs1, rs2\|imm` | `rd = rs1 + op2` | rd |
| 1 | `00001` | `sub` | `rd, rs1, rs2\|imm` | `rd = rs1 − op2` | rd |
| 2 | `00010` | `mul` | `rd, rs1, rs2\|imm` | `rd = low 32 bits of rs1 × op2` | rd |
| 3 | `00011` | `div` | `rd, rs1, rs2\|imm` | signed quotient, truncated toward 0; **exception on ÷0** | rd |
| 4 | `00100` | `mod` | `rd, rs1, rs2\|imm` | signed remainder, sign follows the dividend; **exception on ÷0** | rd |
| 5 | `00101` | `cmp` | `rs1, rs2\|imm` | `E = (rs1 == op2)`, `GT = (rs1 > op2)` signed; no register written | E, GT |
| 6 | `00110` | `and` | `rd, rs1, rs2\|imm` | bitwise AND | rd |
| 7 | `00111` | `or` | `rd, rs1, rs2\|imm` | bitwise OR | rd |
| 8 | `01000` | `not` | `rd, rs2\|imm` | `rd = ~op2` (no `rs1`) | rd |
| 9 | `01001` | `mov` | `rd, rs2\|imm` | `rd = op2` (no `rs1`) | rd |
| 10 | `01010` | `lsl` | `rd, rs1, rs2\|imm` | logical shift left by `op2 mod 32` | rd |
| 11 | `01011` | `lsr` | `rd, rs1, rs2\|imm` | logical shift right (zero fill) by `op2 mod 32` | rd |
| 12 | `01100` | `asr` | `rd, rs1, rs2\|imm` | arithmetic shift right (sign fill) by `op2 mod 32` | rd |
| 13 | `01101` | `nop` | – | does nothing | – |
| 14 | `01110` | `ld` | `rd, imm[rs1]` | `rd = Mem[rs1 + imm]` | rd |
| 15 | `01111` | `st` | `rs, imm[rs1]` | `Mem[rs1 + imm] = rs` (the register is encoded in the `rd` field) | memory |
| 16 | `10000` | `beq` | `label` | branch if `E` | PC |
| 17 | `10001` | `bgt` | `label` | branch if `GT` | PC |
| 18 | `10010` | `b` | `label` | unconditional branch | PC |
| 19 | `10011` | `call` | `label` | `r15 = PC+4`, branch (plus stack push, §9) | r15, PC |
| 20 | `10100` | `ret` | – | return (via `r15`, or via the guarded stack, §9) | PC |
| 21 | `10101` | `halt` | – | stop the machine when it retires | – |
| 22–31 | | *illegal* | | raises an illegal-opcode exception | |

Semantics worth knowing:

* **Arithmetic is 32-bit two's complement and wraps.** No trap on overflow.
* `div` / `mod` follow C semantics: `-17 / 5 = -3`, `-17 % 5 = -2`. `INT_MIN / -1` wraps to `INT_MIN` and `INT_MIN % -1 = 0`.
* **Shift amounts** use only the low 5 bits of `op2` (`lsl r1, r1, 36` ≡ shift by 4).
* `not` and `mov` ignore `rs1`; the shift/arith op uses `rs1` as the value being operated on.
* **Memory operand** is `imm[rs1]`; the assembler also accepts `[rs1]` (offset 0). The offset is an 18-bit signed immediate. The `I` bit is always 1 for `ld`/`st`.
* **Flags**: only `cmp` writes E/GT. All other instructions leave them untouched, so a `cmp` can be followed by several instructions before its `beq`/`bgt`. The ALU also computes N/Z/C/V internally (§7), but they are not architecturally visible.
* **Constants** wider than 18 bits must be built in pieces (e.g. `mov r1, 1` then `lsl r1, r1, 18`). There is no load-upper-immediate and no data directive.
* **Faults** (see §10.3): divide-by-zero, illegal opcode, unaligned or out-of-range data access, plus all stack-guard faults.

### 3.4 Worked encodings

| Assembly | Word | Breakdown |
|---|---|---|
| `add r3, r1, r2` | `0x00C48000` | opcode 0, I=0, rd=3, rs1=1, rs2=2 |
| `mov r0, 10` | `0x4C00000A` | opcode 9 (`01001`) = `0x48000000`, I=1 = `0x04000000`, imm=10 |
| `ld r0, 4[r13]` | `0x74340004` | opcode 14, I=1, rd=0, rs1=13, imm=4 |
| `call` to the next instruction | `0x98000000` | opcode 19, offset 0 |
| `call` 2 instructions back | `0x9FFFFFFE` | offset −2 words (27-bit two's complement) |
| `ret` | `0xA0000000` | opcode 20 |
| `halt` | `0xA8000000` | opcode 21 |
| `nop` | `0x68000000` | opcode 13 |

---

## 4. Assembly language reference

```asm
# comment            ; also a comment
.main:               # label on its own line
    mov r0, 5        # rd, imm
    mov r1, r0       # rd, rs2
loop: cmp r0, 0      # label and instruction on the same line
    beq done
    sub r0, r0, 1
    b loop
done:
    halt
```

| Topic | Rule |
|---|---|
| **Comments** | `;` or `#` to end of line. (`//` is *also* accepted when the source goes through the simulator, because the macro expander strips it first; the standalone assembler does **not** accept `//`.) |
| **Labels** | `name:`. Everything before the **first** colon on a line is the label (trimmed). Names are case-sensitive and are not validated, so `.main`, `loop`, `L1` and even `my label` all work. A label may share a line with an instruction or stand alone (it then points at the next instruction). Duplicate labels are an error. |
| **Mnemonics** | Case-insensitive (`ADD`, `Ld`). |
| **Registers** | `r0`–`r15`, case-insensitive (`R7`). `sp` is **not** an alias in assembly (it is only recognised by the `push`/`pop` macros). |
| **Operands** | Comma-separated. Whitespace is free. |
| **Immediates** | Parsed with C `strtol` base-0 rules: decimal (`10`, `-4`), hex (`0x1F`), octal (a leading `0`, so `010` = 8!). Must fit 18 signed bits. |
| **Memory operands** | `imm[reg]`, `[reg]`, or with spaces `4 [ r13 ]`. |
| **Branch targets** | A label, **or** a numeric absolute *byte* address (`b 8`). The distance must be a multiple of 4 and fit 27 signed bits. |
| **Directives** | None. There is no `.org`, `.word`, `.data`, `.equ` etc. Every non-empty line produces exactly one instruction (4 bytes). |
| **Pseudo-ops** | `push` / `pop` (stack macros, only when assembled through the simulator or the macro expander; see §9.3). |
| **Case of labels in branches** | Exact match required (`Loop` ≠ `loop`). |

Operand shapes per mnemonic (this is the `Format` enum used by both assembler and disassembler):

| Format | Mnemonics | Syntax |
|---|---|---|
| `REG3` | add sub mul div mod and or lsl lsr asr | `op rd, rs1, rs2` or `op rd, rs1, imm` |
| `REG2_NO_RS1` | not mov | `op rd, rs2` or `op rd, imm` |
| `REG2_NO_RD` | cmp | `cmp rs1, rs2` or `cmp rs1, imm` |
| `MEM` | ld st | `op reg, imm[rs1]` |
| `BRANCH` | beq bgt b call | `op label` or `op address` |
| `NO_OPERAND` | nop ret halt | `op` (operands are an error) |

---

## 5. Assembler

`assembler/` → `risc201asm`

### 5.1 Usage

```
risc201asm input.asm [-o outprefix]
```

`outprefix` defaults to `a`. On success three files are written and one line is printed:

| File | Contents |
|---|---|
| `<prefix>.hex` | One lowercase 8-digit hex word per line (`4c00000a`) |
| `<prefix>.bin` | The same words as raw bytes, **big-endian**, 4 bytes per instruction |
| `<prefix>.sym` | `address name` per line (`0000000c .factorial`), **sorted alphabetically by name**, not by address. Empty if there are no labels. |

Exit status 0 on success; 1 on usage error, unreadable input, or any assembly error. On error nothing is written.

### 5.2 Pipeline

```
source text ─▶ Parser ─▶ Lexer ─▶ SourceLine[] ─▶ Pass 1 ─▶ IntermediateRecord[] + SymbolTable
                                                                    │
                                                                    ▼
                                         EncodedRecord[] ◀─ Pass 2 (encode, resolve labels)
```

* **Lexer** (`lexer.cpp`): strips comments, splits `label:` from the rest, lower-cases the mnemonic, keeps the raw operand text.
* **Parser** (`parser.cpp`): numbers lines and feeds each to the lexer; `parseFile` / `parseText`.
* **Pass 1** (`assembler_pass1.cpp`): assigns addresses (every instruction = 4 bytes, starting at 0), defines labels, rejects unknown mnemonics and duplicate labels.
* **Pass 2** (`assembler_pass2.cpp`): encodes each record using the opcode table's `Format`, resolves branch targets, range-checks immediates and offsets.
* **Diagnostics**: errors are collected (not fatal at first error), so one run reports every problem with its source line.
* `Assembler::assembleText(string)` is a stateless in-memory entry point returning `AssembleResult {success, words, symbols, diagnostics}`. A fresh symbol table is built per call (an earlier version leaked state between calls). The simulator uses it directly.

### 5.3 Diagnostics

| Message | Cause |
|---|---|
| `unknown mnemonic 'x'` | not in the opcode table |
| `duplicate label 'x'` | label defined twice |
| `<mn> takes no operands` | operand given to `nop`/`ret`/`halt` |
| `<mn> needs 3 operands: rd, rs1, rs2/imm` | wrong operand count (also the 2-operand and memory variants) |
| `bad register 'x'` | not `r0`–`r15` |
| `bad operand 'x'` | not a register or parseable immediate |
| `bad memory operand 'x'` | malformed `imm[reg]` |
| `immediate out of range` / `offset out of range` | does not fit 18 signed bits |
| `undefined label 'x'` | branch target neither a label nor a number |
| `branch target not 4-byte aligned` | numeric target not ≡ 0 (mod 4) relative to PC+4 |
| `branch offset out of range` | does not fit 27 signed bits |

Example run on a deliberately broken file (9 errors reported in one pass):

```
bad.asm:1: error: unknown mnemonic 'foo'
bad.asm:8: error: duplicate label 'x'
bad.asm:2: error: add needs 3 operands: rd, rs1, rs2/imm
...
9 error(s), no output written.
```

(Pass 1 errors are listed before pass 2 errors, not in line order.)

### 5.4 Example: `assembler/tests/factorial.asm`

Recursive factorial of 10 with a hand-managed stack frame (`r13` = SP, `r15` saved/restored around the recursive `call`). Produces 19 words, with `.main=0x00`, `.factorial=0x0C`, `.continue=0x1C`, `.return=0x44`. Disassembling the result and re-assembling the listing reproduces the identical `.hex`.

---

## 6. Disassembler

`disassembler/` → `risc201disasm`

```
risc201disasm input.hex|input.bin [-o outfile] [-s symfile]
```

* Input type is chosen by extension: `.bin` is read as big-endian 4-byte words; anything else is read as hex text.
* `-s` loads a `.sym` file (`address name` pairs, as written by the assembler) so labels show real names.
* The listing goes to stdout and also to `-o outfile` if given.
* Exit status 0 on success, 1 on error (`error: <what>` on stderr).

### 6.1 Output format

```
.main:
    mov r0, 10    ; 00000000: 4c00000a
    call .factorial    ; 00000004: 98000001
    halt    ; 00000008: a8000000
.factorial:
    cmp r0, 1    ; 0000000c: 2c000001
```

Each line is `mnemonic operands`, then `; <address>: <word>`. The output **re-assembles** to the same words (comments use `;`).

Rules:

* **Labels**: any address that is a symbol or an in-range, word-aligned branch target gets a label line. With no `.sym`, labels are synthesised as `L%08x` (e.g. `L0000000c`).
* **Out-of-range branch targets** (outside the program, e.g. a truncated file) are printed as a **decimal byte address** (`call 12`).
* **Unknown opcodes (22–31)** are emitted as data: `dw 0xb8000000`. (`dw` is not an assembler mnemonic.)
* Immediates are shown signed-decimal; memory operands always as `imm[reg]` including `0[r13]`.
* Register-vs-immediate is chosen from the `I` bit.

### 6.2 Library API (`Disassembler`)

| Method | Purpose |
|---|---|
| `disassemble(path)` | read a file and disassemble it (no symbols) |
| `disassembleWords(words, symbols = {})` | full listing with labels, for use by the debugger |
| `disassembleOne(word, address, symbols = {})` | one instruction, no address/word comment |

### 6.3 Input strictness

The hex reader skips completely empty lines and tolerates trailing `\r` and trailing `# comments` on a data line, but a line containing **only whitespace or only `\r`** makes it fail with `error: stoul`. The simulator's own hex reader is more tolerant (§10.1).

---

## 7. ALU

`alu/` · namespace `risc201` · `#include "alu.hpp"`

The ALU is written to show *how* the arithmetic is done in hardware rather than just using C++ operators. Each operation family has several selectable algorithms, and a separate **reference ALU** built on native C++ operators exists purely as a test oracle.

### 7.1 Interface

```cpp
Alu alu(AluConfig{ /*reference=*/false, AdderKind::KoggeStone,
                   MulKind::Booth4, DivKind::NonRestoring });
AluResult r = alu.execute(AluOp::ADD, a, b, currentFlags);
// r.value  : 32-bit result (0 for CMP)
// r.flags  : AluFlags {E, GT}   – updated only by CMP, otherwise passed through unchanged
// r.nzcv   : AluNzcv {N,Z,C,V}  – computed for ADD/SUB/CMP only (all false for other ops)
```

* `AluOp`: `ADD SUB MUL DIV MOD AND OR NOT LSL LSR ASR MOV CMP`.
* Operands are `int32_t`; results are `uint32_t`.
* `NOT` and `MOV` use operand **B** and ignore A.
* `DIV` / `MOD` by zero throw `AluException("division by zero (div|mod)")` **in both modes**, before dispatch.
* `Alu::mnemonicToAluOp("add")` maps assembler mnemonics to ops and throws `std::invalid_argument` for `ld`, `st`, `nop`, `beq`, `bgt`, `b`, `call`, `ret`, `halt` (loads/stores use `ADD` for address calculation; the others never reach the ALU).
* `Alu::toString(op)` gives the upper-case name.

**Flag semantics for `CMP`** (structural path): `E = Z`, `GT = !Z && (N == V)`, i.e. the classic signed-greater-than from the subtraction flags. The reference path uses `a == b` / `a > b` directly. Both agree on every tested input, including overflow cases.

**NZCV for `ADD`/`SUB`**: `N` = bit 31 of the result; `Z` = result is zero; `C` = carry out (for `SUB` this is "no borrow", i.e. `a >= b` unsigned); `V` = signed overflow.

### 7.2 Selectable algorithms (`AluConfig`)

| Knob | Values | Implementation |
|---|---|---|
| `adder` | `Ripple` | 32 chained full adders (bit-serial carry loop) |
| | `Lookahead` | 8 groups of 4 bits; per-group generate/propagate; two-level carry-lookahead between groups and inside each group |
| | `KoggeStone` | parallel-prefix adder, log₂32 = 5 prefix levels over (generate, propagate) |
| `mul` | `ShiftAdd` | 32 iterations, add `a << i` for each set bit of `b` (through the selected adder) |
| | `Booth4` | radix-4 Booth recoding: 16 digits in {−2,−1,0,+1,+2}; partial products ±a, ±2a through the adder/subtractor |
| `div` | `Restoring` | classic restoring division on magnitudes: shift in a bit, trial-subtract, keep if carry (no borrow) |
| | `NonRestoring` | non-restoring division with a 64-bit signed partial remainder (add or subtract by the remainder's sign) and a final correcting add |
| shifts | – (fixed) | 5-stage **barrel shifter** (shift by 1, 2, 4, 8, 16, each stage enabled by one bit of the 5-bit amount) |
| `reference` | `true` / `false` | `true` bypasses all of the above and uses native C++ arithmetic |

Subtraction is always `a + ~b + 1` through the selected adder. Signed division is built from unsigned division on magnitudes (negation done with the adder); the quotient is negated when the signs differ and the remainder takes the dividend's sign. The library default (`AluConfig()`) is `Ripple / ShiftAdd / Restoring`; the **simulator's** default is `KoggeStone / Booth4 / NonRestoring` (change with `--adder`, `--mul`, `--div`, `--alu-ref`).

### 7.3 Verification

| Binary | What it does | Result |
|---|---|---|
| `risc201alu` (`src/main.cpp`) | 30 hand-written checks: add/sub wrap, cmp flags, flag persistence for non-cmp ops, mul, div/mod sign rules, ÷0 exceptions, AND/OR/NOT, shifts (including `lsl` by 36), MOV, mnemonic mapping | `30 passed, 0 failed` |
| `difftest` (`tests/difftest.cpp`) | Seeded (`mt19937(20260930)`) differential test. Each adder is checked 300 000× for `add` (with random carry-in) and `sub`, sum/carry/overflow against 64-bit arithmetic. Then **all 12 adder × multiplier × divider combinations** are compared to the reference ALU on every op: the full 25×25 edge-value cross product (0, ±1, INT_MIN, INT_MAX, 0x55555555, 0xAAAAAAAA, shift counts 31/32/33/36, …) plus 25 000 random pairs, with random incoming flags. | `5797500 checks, 0 failures` |

`make -C alu test` builds and runs both.

---

## 8. Control unit and pipeline stages

`control/` · namespace `risc201`

Control is **microprogrammed**: a control store holds one microinstruction per opcode, and the decode stage looks it up and turns it into a `ControlPacket` that travels down the pipeline. (An earlier hard-wired control unit was removed in favour of this design.)

### 8.1 Microinstruction and control store

`MicroInstruction` fields (`microinstruction.hpp`):

| Field(s) | Meaning |
|---|---|
| `alu_control` | `NONE ADD SUB MUL DIV MOD AND OR NOT MOV LSL LSR ASR CMP` |
| `branch_control` | `NONE BEQ BGT B CALL RET` |
| `write_back` | `NONE ALU_RESULT MEMORY_DATA PC_PLUS_4` |
| `use_immediate` | operand 2 (or the branch offset) comes from the immediate |
| `memory_read` / `memory_write` | data-memory access |
| `register_write` / `flag_write` | write `rd` / write E,GT |
| `halt` | stop the machine when this instruction retires |
| `defined` | opcode is implemented; undefined opcodes raise an *illegal opcode* fault |
| `pc_write`, `instruction_read`, `stall`, `flush`, `next_address`, `next_control` | declared for a fuller microsequencing model but **not driven** by the current store (direct lookup only) |

`ControlMemory` has 32 entries (one per 5-bit opcode), address = opcode (direct lookup). Opcodes 0–21 are marked `defined`; 22–31 are all-zero and illegal.

| Opc | Instr | ALU | Branch | Write-back | imm | memR | memW | regW | flagW | halt |
|---:|---|---|---|---|:-:|:-:|:-:|:-:|:-:|:-:|
| 0–4 | add sub mul div mod | ADD SUB MUL DIV MOD | – | ALU_RESULT | – | – | – | ✓ | – | – |
| 5 | cmp | CMP | – | – | – | – | – | – | ✓ | – |
| 6–12 | and or not mov lsl lsr asr | AND OR NOT MOV LSL LSR ASR | – | ALU_RESULT | – | – | – | ✓ | – | – |
| 13 | nop | – | – | – | – | – | – | – | – | – |
| 14 | ld | ADD (address) | – | MEMORY_DATA | ✓ | ✓ | – | ✓ | – | – |
| 15 | st | ADD (address) | – | – | ✓ | – | ✓ | – | – | – |
| 16 / 17 / 18 | beq / bgt / b | – | BEQ / BGT / B | – | ✓ | – | – | – | – | – |
| 19 | call | – | CALL | PC_PLUS_4 | ✓ | – | – | ✓ | – | – |
| 20 | ret | – | RET | – | – | – | – | – | – | – |
| 21 | halt | – | – | – | – | – | – | – | – | ✓ |

`MicroProgramSequencer::start(opcode)` selects the entry. It throws `std::out_of_range` for opcode ≥ 32 and `std::invalid_argument("Illegal opcode")` for an undefined entry. `ControlPacket` (`makeControlPacket`) is the subset of the microinstruction that the datapath needs.

### 8.2 Pipeline registers (`pipeline_registers.hpp`)

| Register | Carries |
|---|---|
| `IFIDRegister` | `valid, instruction, pc, pc_plus_4` |
| `IDRRRegister` | + `rs1, rs2, rd, immediate, control` |
| `RREXRegister` | + `operand1, operand2, store_data` (values read from the register file) |
| `EXMARegister` | `alu_result, flags (E,GT), nzcv, store_data, rd, control` |
| `MARWRegister` | `alu_result, memory_data, rd, control` |

All carry a `valid` bit; an invalid register is a pipeline **bubble** and every stage passes it along untouched.

### 8.3 Stages

| Stage | Class | Behaviour |
|---|---|---|
| **IF** | `InstructionFetchStage` | reads the word at `pc` from `InstructionMemory`, sets `pc_plus_4` |
| **ID** | `InstructionDecodeStage` | extracts opcode/`rd`/`rs1`/`rs2`; sign-extends the 18-bit immediate; for opcodes **16–19** replaces it with the sign-extended 27-bit offset; forces `rs1 = 15` for `ret` and `rd = 15` for `call`; looks up the microinstruction; sets `use_immediate` when the `I` bit is 1 on any ALU-class instruction. Throws on an illegal opcode. |
| **RR** | `RegisterReadStage` | reads `rs1`, `rs2` and `rd` (the last supplies `store_data` for `st`) |
| **EX** | `ExecuteStage` | maps `ALUControl` → `AluOp`, picks operand 2 (register or immediate), runs the `Alu`, passes the new E/GT flags along. Branch resolution is done by the simulator driver, not here. |
| **MA** | `MemoryAccessStage` | `ld`: reads `DataMemory[alu_result]`; `st`: writes `store_data` |
| **RW** | `WriteBackStage` | if `register_write`, writes `alu_result`, `memory_data` or `pc_plus_4` (per `write_back`) into `rd` |
| 4-stage wrapper | `FourStageComponents` | `instructionFetch`, `operandFetch` (= ID + RR), `execute`, `memoryWriteback` (= MA + RW) |

Supporting modules:

* **`RegisterFile`**: 16 × 32-bit, bounds-checked (`std::array::at`), `reset()` clears to 0.
* **`InstructionMemory`**: vector of words; `read` throws `std::out_of_range` for unaligned or out-of-range addresses; `load()` replaces the contents.
* **`DataMemory(words = 1024)`**: word-array; `read`/`write` throw `std::runtime_error("Unaligned data memory access")` or `std::out_of_range("Data memory read|write out of range")`; `reset()` zeroes it. The simulator instantiates it with 16 384 words (64 KiB).

### 8.4 Module tests

There is no Makefile for `control/`. Build each test with the ALU sources (see §12 for the command). They are mostly print-and-eyeball programs, some with `assert`:

| Test | Checks |
|---|---|
| `test_control_unit` | microPC == opcode for ADD/LD/BEQ; control bits; `start(32)` throws `out_of_range` |
| `test_instruction_fetch_stage` | fetch returns instruction, pc, pc+4 |
| `test_instruction_decode_stage` | field extraction and control for `add r3,r1,r2` |
| `test_register_read_stage` | operand reads |
| `test_execute_stage` | `ADD` 10+20 → 30, `rd` passthrough |
| `test_memory_writeback` | st → ld → write-back round trip and ALU write-back (asserts) |
| `test_four_stage` | `add r3,r1,r2` through IF→OF→EX→MW, `R3 == 30` (asserts) |

---

## 9. Stack subsystem

`stack/` · namespace `risc201`

A software-visible call stack with **defence in depth**: ergonomic `push`/`pop` macros, hardware-style bounds checking, a stack canary, a shadow stack, precise faults, and a visualiser. Five of its modules (`common`, `fault`, `macro_expander`, `stack_guard`, `stack_visualizer`) are linked into the simulator; `vm`, `demos` and `main` form a separate standalone test harness (§9.6).

### 9.1 Constants (`common.hpp`)

| Constant | Value | Meaning |
|---|---|---|
| `NUM_REGS` | 16 | |
| `SP_REG` | 13 | `r13` is the stack pointer |
| `WORD` | 4 | bytes per word |
| `MEM_SIZE` | `0x10000` | 64 KiB flat data memory |
| `STACK_TOP` | `0x10000` | initial SP (empty stack); highest legal SP |
| `STACK_BASE` | `0xF000` | lowest legal SP → **4 KiB stack** (1024 words) |
| `MAX_IMM` | 2047 | caps the size of one batched push/pop (see §9.3) |

The stack is **full-descending**: `push` = `SP -= 4; Mem[SP] = reg`; `pop` = `reg = Mem[SP]; SP += 4`. Addresses `0x0000`–`0xEFFF` form an ordinary data area.

### 9.2 Call frames

`call` no longer relies solely on `r15`. In stack mode the hardware/simulator pushes a **frame**:

```
 higher addresses
   0x...FFFC  ┌─────────────────────┐
              │ return address      │  ← retSlot  (SP before the call − 4)
              ├─────────────────────┤
              │ canary              │  ← canarySlot (retSlot − 4); random, low byte 0x00
              ├─────────────────────┤  ← SP on entry to the callee
              │ callee's pushed regs│
              │ and locals          │
 lower        └─────────────────────┘
```

* **Canary**: a fresh random 32-bit value per call whose low byte is `0x00` (a "terminator canary": string-style overflows stop at the zero byte), never 0 overall. Seeded from `std::random_device`, so **canary values differ between runs**.
* **Shadow stack**: a separate, un-addressable copy of every active frame's `{retSlot, canarySlot, retAddr, canary, id}`.
* `ret` verifies, in order: a frame exists → **SP == canarySlot** (locals released, nothing over-popped) → **canary intact** → **return address equals the shadow copy**. Only then does it pop the frame, set `SP = retSlot + 4` and jump.
* With `--no-canary` the frame is just the return address (`canarySlot == retSlot`). With `--no-shadow` the return-address comparison is skipped.
* `call` itself also writes `r15 = PC+4`, so code that saves/restores `r15` (as the factorial examples do) keeps working.

### 9.3 `push` / `pop` macro expander (`MacroExpander`)

`push r1, r2, r3` and `pop r3, r2, r1` are **pseudo-instructions** expanded before assembly.

| Mode | `push r1, r2, r3` | `pop r1, r2, r3` |
|---|---|---|
| **Naive** (`--naive`) | `sub r13,r13,4` / `st r1,0[r13]` repeated per register: **6 instructions, 3 SP updates** | `ld r1,0[r13]` / `add r13,r13,4` per register |
| **Batched** (default) | `sub r13,r13,12` / `st r1,8[r13]` / `st r2,4[r13]` / `st r3,0[r13]`: **4 instructions, 1 SP update** | `ld r1,0[r13]` / `ld r2,4[r13]` / `ld r3,8[r13]` / `add r13,r13,12` |

For `n` registers, batching emits `n+1` instructions instead of `2n` and does 1 SP update instead of `n`. The first-listed pushed register ends up at the highest address, so `push r1,r2,r3` followed by `pop r3,r2,r1` restores everything (LIFO).

**Batching rules**: consecutive `push` lines (or consecutive `pop` lines) merge into one batch. A batch is **broken** by a label, by any non-stack instruction, by a push↔pop switch, or when its size would exceed the immediate cap (the total must stay ≤ `MAX_IMM` = 2047 bytes, so a batch holds at most 511 registers; a longer list is split into several batches). Blank lines and comment-only lines do *not* break a batch. The statistics (`ExpandStats`: naive vs emitted instruction count, SP-update count, batch count) are printed by the standalone demos (when the program uses `push`/`pop`), when the interactive debugger loads a program, and by the debugger's `macro` command.

**Validation** (all raise `Fault::MacroError` at expansion time, reported against the original source line):

* `sp` or `r13` as an operand (`sp (r13) cannot be used in push`, `r13 is the stack pointer and cannot be pushed|popped`)
* malformed register (`invalid register 'x'`), out of range (`register r99 does not exist (r0..r15)`)
* duplicate destination in one `pop` (`duplicate destination register r1 in pop`); duplicates in `push` are legal
* empty operand list (`push needs at least one register`)

**Source-line map**: each emitted instruction remembers the original line (`AsmLine::srcLine`). The combined `sub`/`add r13` of a batch is attributed to the *last* push/pop line, so an overflow fault points at the push that actually crossed `STACK_BASE`. Comments `;`, `#` and `//` are stripped, mnemonics are case-insensitive, and every other line is passed through unchanged (with labels emitted as their own `name:` line).

### 9.4 `StackGuard` and the fault model

Hook points (called from the simulator's memory stage, or from any CPU model):

| Hook | When | Checks |
|---|---|---|
| `checkSP(newSP)` | before any write to `r13` commits | alignment → below base → above top → inside the current frame's header |
| `checkAccess(addr, store, sp, viaSP)` | before every `ld`/`st` | alignment → below-SP → region bounds → wild pointer |
| `pushFrame(retSlot, retAddr)` | on `call` | registers a frame, returns the canary |
| `verifyReturn(sp, canaryInMem, retInMem)` | on `ret` | frame exists, SP balanced, canary, shadow copy |

`viaSP` is true when the base register is `r13` **or holds a value derived from SP**. The guard tracks this **SP provenance** per register: `mov r2, r13`, `add r2, r13, 8` and `sub r2, r13, 8` produce SP-derived values; `ld`, `and`, `mul` etc. clear it; `sub r2, r13, r13` (pointer difference, SP minus SP-derived) is a plain number, not a stack pointer. This lets the guard report a pointer-into-stack that runs off the end as a **stack overflow** instead of a generic out-of-bounds access (demo 15 vs demo 9).

#### The 12 faults (`fault.hpp`)

| Fault | Printed name | Raised when |
|---|---|---|
| `StackOverflow` | `STACK_OVERFLOW` | SP would drop below `0xF000`; or an SP-derived access lands below the base |
| `StackUnderflow` | `STACK_UNDERFLOW` | SP would exceed `0x10000` (pop on an empty stack); or an SP-derived access lands at/above the top |
| `MisalignedSP` | `MISALIGNED_SP` | an SP write that is not a multiple of 4 |
| `MisalignedAccess` | `MISALIGNED_ACCESS` | `ld`/`st` address not word-aligned |
| `BelowSP` | `ACCESS_BELOW_SP` | in-stack access below SP, i.e. freed memory |
| `OutOfBounds` | `OUT_OF_BOUNDS` | a non-stack-derived pointer outside `[0, 0x10000)` |
| `CanaryCorrupted` | `CANARY_CORRUPTED` | canary differs at `ret` |
| `ShadowStackMismatch` | `SHADOW_STACK_MISMATCH` | saved return address differs from the shadow copy |
| `ShadowStackEmpty` | `SHADOW_STACK_EMPTY` | `ret` with no matching `call` |
| `UnbalancedFrame` | `UNBALANCED_FRAME` | SP ≠ frame base at `ret`, or SP rises into its own frame header |
| `BadInstruction` | `BAD_INSTRUCTION` | parse error, duplicate/undefined label (VM), jump outside the program, step limit, **return to an invalid address** (simulator, when both defences are off) |
| `MacroError` | `MACRO_ERROR` | push/pop validation failure |

`StackException` carries `{fault, message, addr, srcLine, pc, srcText}`; the original source line and instruction text are filled in by the executor.

### 9.5 Stack visualiser (`renderStack`)

An ASCII frame renderer, available as the debugger's `stack` command, printed automatically on any stack fault, and as `viz` in the standalone VM. Each row shows **address | word | SP-offset | role**:

```
  STACK MEMORY  (full-descending: push = SP-=4 then store; grows to lower addresses)
  +--------------------------------------------------------------------------------+
  | == TOP of stack region 0x00010000  (empty-stack SP) ==                         |
  | 0x0000FFFC | 0x0000000C | SP+4   | RET    f1 -> pc 12                          |
  | 0x0000FFF8 | 0xAE3ED600 | SP     | CANARY f1  <== SP                           |
  |    free: 4088 bytes below SP                                                   |
  | == BASE of stack region 0x0000F000  (overflow limit) ==                        |
  SP=0x0000FFF8  used 8/4096 B  [........................................]  frames=1  shadow=on  canary=on
  shadow stack (top first):  f1->pc 12
```

Roles: `RET fN -> pc X`, `CANARY fN`, `fN locals / saved regs`, `data`. Corruption is flagged live: `!! RET OVERWRITTEN fN (was pc X)` and `!! CANARY SMASHED fN`. If more than 24 words are in use it prints the first 4 and last 20 with a `... N more word(s) ...` line. A 40-character usage bar and the shadow-stack contents (top first) close the dump. (In the standalone VM `pc` is an *instruction index*; in the simulator it is a *byte address*.)

### 9.6 Standalone harness: `risc201_stack`

A minimal VM (`vm.cpp`) that exists to exercise the stack subsystem in isolation. It is **not** the real CPU: it parses its own tiny instruction subset (`add|sub rd, rs, rt|imm`, `mov`, `ld`/`st rd, imm[rs]`, `call label`, `jmp label`, `ret`, `halt`, plus the pseudo-instruction `viz`), keeps 64 KiB of little-endian byte memory, stores return addresses as *instruction indices*, and stops after 100 000 steps (`step limit exceeded`).

```
risc201_stack                    run the 18 built-in demos
risc201_stack prog.s             macro-expand and run your own file
flags: --naive   --no-shadow   --no-canary   -v (verbose: show expansion + stack dump on fault)
```

Exit status: 0 if every demo behaved as expected (or the file ran to `halt`), 1 otherwise, 2 if the file cannot be opened.

### 9.7 The 18 built-in demos

Expectations adapt to the enabled defences; all 18 pass under default flags, `--naive`, `--no-canary`, and `--no-canary --no-shadow` (verified).

| # | Demo | Default expectation |
|--:|---|---|
| 1 | push/pop round trip + call, `viz` inside callee | halts, r1/r2/r3 = 11/22/33 |
| 2 | nested calls (two frames on the visualiser) | halts, r1 = 1 |
| 3 | linear buffer overflow one word past `buf[8]` | `CANARY_CORRUPTED` (→ `SHADOW_STACK_MISMATCH` with `--no-canary`; `BAD_INSTRUCTION` with both off) |
| 4 | direct return-address overwrite skipping the canary | `SHADOW_STACK_MISMATCH` (halts normally with `--no-shadow`) |
| 5 | `pop` on empty stack | `STACK_UNDERFLOW` |
| 6 | three `sub r13,r13,2000` | `STACK_OVERFLOW` |
| 7 | `sub r13,r13,3` | `MISALIGNED_SP` |
| 8 | store below SP after releasing a frame | `ACCESS_BELOW_SP` |
| 9 | `mov r2,0x20000` + `ld` | `OUT_OF_BOUNDS` |
| 10 | `ret` with no call | `SHADOW_STACK_EMPTY` |
| 11 | function leaves SP unbalanced | `UNBALANCED_FRAME` |
| 12 | `push r1, r13` | `MACRO_ERROR` |
| 13 | `pop r1, r99` | `MACRO_ERROR` |
| 14 | `pop r1, r1` | `MACRO_ERROR` |
| 15 | SP copied to `r2`, walked off the base, `ld` | `STACK_OVERFLOW` (SP provenance) |
| 16 | `pop` inside a function with nothing pushed | `UNBALANCED_FRAME` |
| 17 | `sub r2,r13,r13` then `st r1,0[r2]` | halts (pointer difference is not an SP pointer) |
| 18 | duplicate label | `BAD_INSTRUCTION` |

---

## 10. Simulator and debugger

`Simulator/simulator.cpp` → `risc201sim`

The control module supplies the individual stages; `simulator.cpp` is the **driver** that owns the pipeline registers, the PC, forwarding, hazard detection, branch resolution, statistics, the stack-guard hooks and the debugger. It runs one clock cycle at a time (`Simulator::step`) and processes stages **back-to-front** within a cycle, so each stage consumes its input register before the stage behind it overwrites it (equivalent to a single clock edge).

### 10.1 Command line

```
risc201sim [options] <program>
```

| Option | Effect | Default |
|---|---|---|
| `--pipeline 4\|6` | pipeline depth | `6` |
| `--alu-ref` | use the reference ALU (native C++ arithmetic) | structural ALU |
| `--adder ripple\|cla\|kogge` | adder algorithm | `kogge` |
| `--mul shiftadd\|booth` | multiplier | `booth` |
| `--div restoring\|nonrestoring` | divider | `nonrestoring` |
| `--batch` | run to completion, print **one** summary line, no interactive session | interactive |
| `--naive` | naive (non-batched) `push`/`pop` expansion | batched |
| `--no-shadow` | disable the shadow stack | on |
| `--no-canary` | disable the stack canary | on |
| `--no-stack` | disable the whole stack subsystem: plain link-register `call`/`ret`, no guard, no frames | on |
| `<program>` | `.asm` or `.s` → macro-expand → assemble → run. **Any other extension is read as a hex file** (one word per line; `//` and `#` comments and blank lines allowed). | required |

An unknown option, a bad value, or a missing program prints the usage text to stderr and exits 1. All ALU choices give **identical architectural results** (verified by `difftest`); they differ only in how the arithmetic is computed.

For `.asm`/`.s` input the simulator reports assembler errors against the **original** source lines (before macro expansion) and loads nothing if any exist.

### 10.2 Pipelines

```
6-stage :  IF ─▶ ID ─▶ RR ─▶ EX ─▶ MA ─▶ RW
           fetch decode read  ALU/  memory write
                        regs  branch      back

4-stage :  IF ─▶ OF (ID+RR) ─▶ EX ─▶ MW (MA+RW)
```

| Mechanism | 6-stage | 4-stage |
|---|---|---|
| **Register file timing** | write-back happens before register read in the same cycle (write-first) | same |
| **Forwarding** | EX/MA → EX (newest wins) and MA/RW → EX | EX/MW → EX only |
| **Forwarded operands** | `rs1`, `rs2`, and the `st` data register, **only if the instruction really reads them** (`not`/`mov` have no `rs1`; immediate forms have no `rs2`; branch offset bits are never treated as registers) | same |
| **Forwarded value** | ALU result, or PC+4 for `call` | same |
| **Load-use hazard** | a `ld` in EX whose `rd` is read by the instruction in RR (ID/RR) → **1 stall cycle** (bubble into EX, IF/ID/RR held) | the instruction being decoded in OF stalls 1 cycle |
| **`ret` after `ld r15`** | `ret` reads `r15`, so it is a dependent of the load → stalls (test `ret_after_load`) | same |
| **Taken branch** (`b`, `beq`, `bgt`, `call`) | resolved in **EX**; the two younger instructions are squashed → **2 bubbles** | resolved in EX; 1 younger instruction squashed → **1 bubble** |
| **`ret`, link-register mode** (`--no-stack`) | PC ← `r15`, resolved in EX → 2 bubbles | 1 bubble |
| **`ret`, stack mode** (default) | target comes from the guarded stack at **MA** → 3 bubbles | 2 bubbles |
| **Branch prediction** | none (fall-through is always fetched, squashed if taken) | none |
| **Flags** | written in EX when `flag_write`; a following `beq`/`bgt` sees them next cycle, so `cmp`→branch never stalls | same |

Because there is no speculation beyond the squash, the cycle count is predictable:

```
cycles = instructions_retired + (stages − 1) + load_use_stalls + Σ branch_penalties
```

Check with `fibonacci` on the 6-stage pipe: 76 retired + 5 + 0 stalls + 11 taken branches × 2 = **103**; on the 4-stage: 76 + 3 + 11 × 1 = **90**. (`HALT` itself counts as a retired instruction.)

### 10.3 Execution and stop conditions

The machine stops when one of these happens (the text is the `stop=` / "Program stopped:" reason):

| Reason | Meaning |
|---|---|
| `HALT` | a `halt` reached the final stage (RW, or MW in the 4-stage pipe) |
| `end of program reached (no HALT executed)` | PC ran past the last instruction and the pipeline drained |
| `EXCEPTION <NAME>: <msg>` | a stack-guard fault (§9.4) |
| `EXCEPTION: division by zero (div\|mod) at PC 0x…` | ALU exception |
| `EXCEPTION: Illegal opcode (opcode 22 = 0x16) at PC 0x…` | decode of opcode 22–31 |
| `EXCEPTION: Unaligned data memory access (address …, PC …)` / `Data memory read\|write out of range …` | raw memory faults (stack mode off) |
| `EXCEPTION: unaligned instruction fetch at 0x…` | PC not a multiple of 4 |
| `cycle limit (1000000) reached - possible infinite loop` | safety net |

An instruction counts toward `instructions` when it retires (reaches the last stage), so a faulting instruction is never counted. Where the fault is detected matters for how much older work has finished:

* **Stack-guard faults** are raised in the **memory stage**, the first point at which every older instruction has fully retired its register and memory effects. They are therefore **precise**: nothing younger has executed and nothing older is lost, and the report carries the original source line and instruction text.
* **ALU faults** (divide by zero, found in EX) and **illegal opcodes** (found in ID/OF) halt the machine immediately, so results of older instructions still in flight can be missing from the final register state (e.g. `fault_divzero` reports `instructions=1` on the 6-stage pipe but `2` on the 4-stage pipe).

### 10.4 Memory map and stack integration

* Data memory is exactly `[0, 0x10000)` (16 384 words). The stack occupies `[0xF000, 0x10000)`; `r13` starts at `0x10000`.
* Instruction memory is separate and holds only the program, loaded at address 0. The simulator never lets the program read or write its own code.
* With the stack subsystem **on** (default), the memory stage runs the guard hooks for every instruction: `call` pushes `retSlot` (+ canary) and updates `r13`; `ret` verifies and pops; `ld`/`st` go through `checkAccess`; any instruction writing `r13` goes through `checkSP`; SP provenance (`sp_derived[]`) is maintained per register. A `ret` whose verified target is outside the program or unaligned raises `BAD_INSTRUCTION: return to invalid address 0x…`.
* With `--no-stack`, none of this exists: `call` just sets `r15` and jumps, `ret` jumps to `r15`, and bad accesses surface as raw memory exceptions.
* Programs written for the link-register convention that leave an unbalanced stack (e.g. `ret_after_load.asm`) are only valid with `--no-stack`; in stack mode they raise `UNBALANCED_FRAME`.

### 10.5 `--batch` output

One line, space-separated `key=value`:

```
pipeline=6 alu=KoggeStone/Booth4/NonRestoring cycles=100 instructions=60 cpi=1.667 flushes=15 stalls=0 stop="HALT" r0=5 r1=120 r2=0 … r13=65536 r14=0 r15=12
```

* `alu=` is `reference` or `adder/mul/div`.
* `cpi` = cycles ÷ retired instructions, 3 decimals.
* `flushes` counts taken branches/`call`s/`ret`s (squash events); `stalls` counts load-use stall **cycles**.
* If a stack fault stopped the machine, `srcline=N` follows `stop=` (the original source line).
* `r0`…`r15` are printed as **unsigned decimal** (`r13=65536` is `0x10000`).
* Exit status is **0 even when the program faults** (the fault is in `stop=`); a macro-expansion fault prints `stop="EXCEPTION MACRO_ERROR: …" srcline=N` and also exits 0. Non-fault errors (cannot open file, assembler errors) print `Error: …` to stderr and exit 1.

### 10.6 Interactive debugger

Started when `--batch` is absent. After loading it prints the program size, pipeline, ALU, stack options and macro statistics, then the `(dbg)` prompt.

| Command | Alias | Action |
|---|---|---|
| `step [N]` | `s` | run N clock cycles (default 1), then show the machine state |
| `run` | `r`, `c`, `continue` | run until halt or breakpoint, then show state |
| `break <hex_addr>` | `b` | breakpoint at a 4-byte-aligned address (hex with or without `0x`) |
| `delete <hex_addr\|all>` | `d` | remove one / all breakpoints |
| `info break` | `i` | list breakpoints (any `i …` works) |
| `regs` | | redisplay pipeline + registers |
| `print <Rn\|hex_addr>` | `p` | one register, or one data-memory word |
| `mem <hex_addr> [N]` | `x` | dump N data-memory words (default 8; N of 0 or >4096 is reset to 8) |
| `micro [hex_opcode]` | `m` | control-store entry for an opcode (default: last decoded) |
| `stack` | `k` | draw the stack frames (§9.5) |
| `macro` | | `push`/`pop` expansion statistics |
| `reset` | | reload the program from the file (breakpoints kept; **canary values change**) |
| `help` | `h` | command list |
| `quit` | `q` | exit |

**State display** (after `step`/`run`/`regs`):

```
=== Cycle 9 (6-stage) ===  [STALL: load-use hazard]  [FLUSH: branch taken]
Stage | PC         | Detail
------+------------+--------------------------------
IF/ID | 0x0000001c | instr 0x9000000a
ID/RR | 0x00000018 |   opcode 0x11  uPC=0x11  rd R0  rs1 R0  rs2 R0  I=0
RR/EX | 0x00000014 | val1 0x00000005  val2 0x00000005
EX/MA | 0x00000010 | ALU result 0x00000000
MA/RW |    ----    | write data 0x00000000
PC: 0x00000020   Flags: E=0 GT=1
R0 =0x00000005  R1 =0x00000001 ...
```

The 4-stage pipe labels the registers `IF/OF`, `OF/EX`, `EX/MW` and has no ID/RR or MA/RW rows. `----` marks a bubble. When the program stops, the debugger also prints the stop reason, any stack-fault report (with source line and an automatic stack dump), and an **execution summary**:

```
=== Execution Summary ===
Pipeline:        6-stage
ALU:             KoggeStone/Booth4/NonRestoring
Total Cycles:    100
Instructions:    60
CPI:             1.667
Branch Flushes:  15
Load-Use Stalls: 0
```

**Breakpoint semantics**: a breakpoint fires when the instruction is *about to execute*, i.e. it sits in the RR/EX (OF/EX) register, and execution stops **before** that instruction runs (`Breakpoint hit at 0x… (about to execute)`). A breakpoint is not re-triggered on the very first cycle after you resume from it. Breakpoints are honoured by both `step N` and `run`.

**`micro` output** shows the control-store row for the opcode (`uPC`, memory/register/flag controls, ALU/branch/write-back enum values as integers, halt); illegal opcodes print a `MICROCODE EXCEPTION`. Because the store is a direct lookup, every trace has exactly one step.

---

## 11. Test programs and expected results

`Simulator/tests/` — results below were reproduced by running the programs. Cycle numbers assume default ALU settings (they do not depend on the ALU choice).

| Program | What it exercises | Expected |
|---|---|---|
| `factorial5.asm` | recursion, `call`/`ret`, hand-made frames, `mul` | `r1=120`, `r0=5`, `r13=0x10000` restored. 6-stage: **100 cycles / 60 instr / 15 flushes / 0 stalls**; 4-stage: **83 cycles** |
| `fibonacci.asm` | loop, `cmp`/`beq`/`b`, register moves | `r2=55` (`r3=r4=89`). 6-stage: **103 / 76 / 11 / 0**; 4-stage: **90** |
| `loaduse.asm` | `st`→`ld` forwarding, load-use stalls, store-data forwarding, `mul` | `r3=7 r4=14 r5=14 r6=7 r7=49`. 6-stage **17 cycles, 2 stalls**; 4-stage **15 cycles, 2 stalls** |
| `ret_after_load.asm` | `ret` right after `ld r15` must stall (**run with `--no-stack`**) | `r5=99`, 1 stall, 2 flushes. 6-stage 18 cycles / 8 instr; 4-stage 14 cycles |
| `fault_divzero.asm` | `div` by zero | `EXCEPTION: division by zero (div) at PC 0x00000008` |
| `fault_range.asm` | `ld` from `1 << 18` (0x40000) | stack mode: `EXCEPTION OUT_OF_BOUNDS`; `--no-stack`: `Data memory read out of range (address 0x00040000, PC 0x00000008)` |
| `fault_unaligned.asm` | `ld` from address 2 | stack mode: `EXCEPTION MISALIGNED_ACCESS`; `--no-stack`: `Unaligned data memory access (address 0x00000002, PC 0x00000004)` |
| `fault_illegal_opcode.hex` | raw words `0xb0000000` (opcode 22) and `0xc8000000` (opcode 25); no `.asm` | `EXCEPTION: Illegal opcode (opcode 22 = 0x16) at PC 0x00000000` |

Each `*.hex` / `*.sym` beside an `.asm` is the assembler's output for it. The header comment inside `factorial5.asm` says `r13 restored (0x1FFFC)`; the actual value is `0x10000` (see §13).

### Stack/fault programs (`Simulator/tests/stack/`)

Eighteen small `.s` programs, one per fault scenario. Each starts with `; EXPECT: <FAULT_NAME | OK | ASM_ERROR>` and optionally `; REGS: r1=11 r2=22`:

| File | Expect | File | Expect |
|---|---|---|---|
| `01_roundtrip_call` | OK (`r1=11 r2=22 r3=33`) | `10_ret_without_call` | `SHADOW_STACK_EMPTY` |
| `02_nested_calls` | OK (`r1=1`) | `11_unbalanced_at_ret` | `UNBALANCED_FRAME` |
| `03_linear_overflow` | `CANARY_CORRUPTED` | `12_macro_push_sp` | `MACRO_ERROR` |
| `04_retaddr_overwrite` | `SHADOW_STACK_MISMATCH` | `13_macro_bad_reg` | `MACRO_ERROR` |
| `05_pop_empty` | `STACK_UNDERFLOW` | `14_macro_dup_pop` | `MACRO_ERROR` |
| `06_runaway_alloc` | `STACK_OVERFLOW` | `15_sp_alias_overflow` | `STACK_OVERFLOW` |
| `07_misaligned_sp` | `MISALIGNED_SP` | `16_pop_into_header` | `UNBALANCED_FRAME` |
| `08_below_sp` | `ACCESS_BELOW_SP` | `17_pointer_difference` | OK |
| `09_wild_pointer` | `OUT_OF_BOUNDS` | `18_duplicate_label` | `ASM_ERROR` (assembler `duplicate label`) |

They mirror the 18 demos of the standalone harness, adapted to the real ISA (e.g. test 09 loads from `98304` = `0x18000` because `0x20000` does not fit an 18-bit immediate).

---

## 12. Testing everything

All of the following pass on the current sources (verified on Linux/g++).

```sh
# ALU: 30 directed checks + 5,797,500 randomized differential checks
make -C alu test

# Stack subsystem standalone demos (18/18), also with defence variants
cd stack
./risc201_stack ; ./risc201_stack --naive ; ./risc201_stack --no-canary ; ./risc201_stack --no-canary --no-shadow

# Control-unit module tests (no Makefile; compile each with the ALU sources)
cd control
for t in tests/*.cpp; do
  g++ -std=c++17 -Iinclude -I../alu/include -o /tmp/$(basename $t .cpp) \
      $t src/*.cpp ../alu/src/alu.cpp ../alu/src/units.cpp && /tmp/$(basename $t .cpp)
done

# Pipeline simulator: every Simulator/tests/*.asm on both pipelines
cd Simulator && make && sh tests/run_tests.sh

# Stack/fault programs through the real simulator: 23 checks
python3 tests/stack_tests.py            # optional arg: path to risc201sim
```

`run_tests.sh` runs each `*.asm` with `--pipeline 6` then `--pipeline 4` (`ret_after_load` with `--no-stack`) and prints the first fields of the summary line. Override the binary with `SIM=/path/to/risc201sim`. It does not run `fault_illegal_opcode.hex`.

`stack_tests.py` runs each `stack/*.s` on **both pipelines × batched and naive expansion**, parses `stop="…"` and the register values from the batch line, plus two flag-variant cases (`03_linear_overflow` with `--no-canary` → `SHADOW_STACK_MISMATCH`, with `--no-canary --no-shadow` → `BAD_INSTRUCTION`) and three **batched == naive** register-equality checks for programs that finish (`r15` is excluded because return addresses legitimately differ when batching shortens the code). Exit status 1 on any failure. Output ends `23/23 checks passed`.

---

## 13. Known issues, quirks and stale artefacts

Observations from reading and running the code; none block normal use.

**Line endings.** In this checkout the files have Windows CRLF endings (git's stored blobs are LF). `Simulator/tests/run_tests.sh` and `stack_tests.py` therefore fail on Linux/macOS (`cd: can't cd to .` / `Syntax error`) until converted: `sed -i 's/\r$//' Simulator/tests/run_tests.sh Simulator/tests/stack_tests.py` (or re-checkout with `core.autocrlf` off). `git status` shows every text file as modified for the same reason.

**Assembler**
* `isRegisterToken` calls `std::stoi` on the digits after `r`, so an absurd register such as `r99999999999` throws an uncaught `std::out_of_range` and **aborts** the assembler instead of reporting `bad register`.
* The standalone assembler accepts only `;` and `#` comments; `//` works only through the simulator's macro-expander front end.
* Leading-zero numbers are octal (`010` = 8).
* No directives, no data section, no way to preload data memory; constants are limited to 18 bits per instruction.
* `.sym` is written in alphabetical order of name, not address order.

**Disassembler**
* A whitespace-only or CR-only line in a `.hex` file aborts with `error: stoul` (the assembler never produces one, but hand-edited files might).

**Stale or misleading artefacts**
* `assembler/tests/factorial_reconstructed_bin.asm` / `_hex.asm` are snapshots from before `halt` existed and before `r13` became the SP: they show `nop` where `halt` is now and use `r14` as the stack pointer. Re-running the current disassembler produces `halt` and `r13`.
* `Simulator/tests/factorial5.asm` header says `r13 restored (0x1FFFC)`; the machine now ends with `r13 = 0x10000`.
* `stack/include/common.hpp`'s header comment assumes a **12-bit** immediate; the real ISA has an **18-bit** immediate. `MAX_IMM = 2047` is therefore only a (conservative) cap on how many registers one batched `push`/`pop` may cover.
* The `viz` and `jmp` instructions exist only in the standalone `risc201_stack` VM; they are not assembler mnemonics (use `b`, and the debugger's `stack` command).
* `.exe` files and `stack/build/*.o` are committed Windows build outputs and may lag the sources; rebuild before trusting them. `.vscode/c_cpp_properties.json` contains a developer-specific compiler path.
* `stack/Makefile` is Windows `cmd.exe`-only (`if not exist`, `.\risc201_stack.exe`).

**Hardware-model notes**
* Microinstruction fields `pc_write`, `instruction_read`, `stall`, `flush`, `next_address`, `next_control` are declared but unused; hazards, flushes and PC updates are handled by the simulator driver rather than by microcode.
* The ALU computes N, Z, C, V, but the machine stores and uses only E and GT; `bgt` is *signed* greater-than, and there is no unsigned compare or branch-on-less-than (use swapped operands or `bgt`/`beq` combinations).
* `call` writes `r15` even in stack mode, and `ret` ignores it in stack mode, so `r15` saving in nested code is harmless but not required there.
* Canary values are random per `call` and per run, so memory dumps containing a canary differ between runs; behaviour (faults, results, cycle counts) does not.
* `ret` in stack mode costs one more cycle than a link-register `ret` because it is resolved at MA.

---

## 14. Project history and contributors

Repository `github.com/GASKET-SEAL/risk201`, 37 commits reachable from `HEAD`, between 2026-09-25 and 2026-11-05 across branches `main`, `assembler` (currently checked out) and `dhruvi-microprogrammed-control`. By commit authorship the work divides roughly as:

| Area | Contributor(s) in git history |
|---|---|
| ALU (structural arithmetic, differential tests) | Sejal Sharma |
| Control unit, pipeline stages, 4-stage components | Dhruvi Shah |
| Stack subsystem (macros, guard, faults, visualiser) | Somya Kumar |
| Assembler, disassembler (incl. `.sym` support and the symbol-aware disassembly API), `.gitattributes` | Parth / `partth-somanni` (Partth Somanni) |
| Simulator and debugger | commits recorded under the author name `unknown` (no identity configured on that machine); the `assembler`-branch merge commits carry the same name |

Notable milestones: ALU prototype → rewritten from operator-based to algorithm-based; hard-wired control replaced by a microprogrammed control store; assembler/disassembler validated on `factorial.asm`; `.sym` debug output and `parseText()` added for the debugger; `halt` opcode (21) added; the original standalone `stack.cpp` split into the modular `stack/` library; simulator extended to take `.asm` directly and to integrate the stack guard on both pipelines.

*Course: CSC-201, Computer Organization & Architecture.*
