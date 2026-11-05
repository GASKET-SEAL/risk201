; EXPECT: OK
; REGS: r1=1
        mov r1, 1
        call outer
        halt
    outer:
        push r1
        mov r1, 2
        call inner
        pop r1
        ret
    inner:
        push r1, r2
        pop r2, r1
        ret
