#!/bin/sh
# Runs every tests/*.asm directly on both pipelines (the simulator expands macros and assembles).
# ret_after_load.asm is a pure link-register program, so it runs with --no-stack.
# Stack/fault tests:  python3 tests/stack_tests.py
cd "$(dirname "$0")"
SIM=${SIM:-../risc201sim}
for f in *.asm; do
  n=${f%.asm}; flags=""
  [ "$n" = ret_after_load ] && flags="--no-stack"
  for p in 6 4; do
    printf "%-16s %s-stage: " "$n" "$p"
    "$SIM" --pipeline $p $flags --batch "$f" | cut -d' ' -f3-9
  done
done
