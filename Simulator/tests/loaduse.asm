# Load-use + store-data forwarding. Expect r3=7 r4=14 r5=14 r6=7 r7=49; 6-stage 17 cycles, 2 stalls; 4-stage 15 cycles
    mov r1, 100
    mov r2, 7
    st r2, [r1]
    ld r3, [r1]
    add r4, r3, r3
    st r4, 4[r1]
    ld r5, 4[r1]
    sub r6, r5, r2
    mul r7, r6, r6
    halt
