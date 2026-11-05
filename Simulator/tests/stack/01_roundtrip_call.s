; EXPECT: OK
; REGS: r1=11 r2=22 r3=33
        mov r1, 11
        mov r2, 22
        mov r3, 33
        push r1, r2, r3        ; one batch
        mov r1, 0
        mov r2, 0
        mov r3, 0
        pop r3, r2, r1         ; LIFO: reverse order restores the registers
        call fn
        halt
    fn:
        push r1                ; three separate lines still merge
        push r2
        push r3
        mov r1, 7
        pop r3
        pop r2
        pop r1
        ret
