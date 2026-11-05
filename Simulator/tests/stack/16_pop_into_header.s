; EXPECT: UNBALANCED_FRAME
call f
halt
f:
pop r1
ret
