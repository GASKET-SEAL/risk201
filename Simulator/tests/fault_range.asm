# Fault: data address out of range
mov r1, 1
lsl r1, r1, 18
ld r2, [r1]
halt
