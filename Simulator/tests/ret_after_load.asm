# RET immediately after ld r15 must stall. Expect r5=99 and 1 stall
.main:
    call .f
    mov r5, 99
    halt
.f:
    sub r13, r13, 4
    st r15, [r13]
    mov r15, 0
    ld r15, [r13]
    ret
