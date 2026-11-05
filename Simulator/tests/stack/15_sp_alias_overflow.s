; EXPECT: STACK_OVERFLOW
mov r2, r13
sub r2, r2, 2044
sub r2, r2, 2044
ld r1, -12[r2]
halt
