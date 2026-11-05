; EXPECT: OUT_OF_BOUNDS
mov r2, 98304         ; 0x18000, outside memory (0x20000 does not fit imm18)
ld r1, 0[r2]
halt
