; EXPECT: UNBALANCED_FRAME
call f
halt
f:
sub r13, r13, 4
ret
