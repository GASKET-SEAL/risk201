#!/bin/sh
# Run from the risk201 root:  sh fpu/tests/run_sim_test.sh
set -e
make -s -C assembler
make -s -C Simulator
ASM=assembler/risc201asm
SIM=Simulator/risc201sim
$ASM fpu/tests/fpu_sim.asm -o /tmp/fpu_sim
OUT=$(printf 'run\nquit\n' | $SIM /tmp/fpu_sim.hex)

fail=0
check() {   # reg  expected
    if echo "$OUT" | grep -q "R$1 *=$2"; then echo "PASS r$1 = $2"
    else echo "FAIL r$1 expected $2"; fail=1; fi
}
check 1  0x40400000
check 2  0x40000000
check 3  0x40a00000
check 4  0x3f800000
check 5  0x40c00000
check 6  0x3fc00000
check 7  0x3eaaaaab
check 8  0x00000001
check 9  0xc0e00000
check 10 0xfffffff9
check 11 0x00000001
check 12 0x00000001
exit $fail
