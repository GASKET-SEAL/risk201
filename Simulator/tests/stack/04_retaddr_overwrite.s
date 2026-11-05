; EXPECT: SHADOW_STACK_MISMATCH
        call victim
        halt
    victim:
        sub r13, r13, 8
        mov r1, 8
        st r1, 12[r13]         ; direct write to the saved return address, skipping the canary
        add r13, r13, 8
        ret
        halt
