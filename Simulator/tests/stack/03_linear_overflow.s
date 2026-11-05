; EXPECT: CANARY_CORRUPTED
        call victim
        halt
    victim:
        sub r13, r13, 8        ; char buf[8]
        mov r1, 255
        st r1, 0[r13]
        st r1, 4[r13]
        st r1, 8[r13]          ; one word too far -> canary
        add r13, r13, 8
        ret
