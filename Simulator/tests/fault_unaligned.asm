# Fault: unaligned load
mov r1, 2
ld r2, [r1]
halt
