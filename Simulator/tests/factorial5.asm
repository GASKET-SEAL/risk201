# Recursive factorial(5) through call/ret + stack. Expect r1=120, r0=5, r13 restored (0x1FFFC)
.main:
    mov r0, 5
    mov r1, 1
    call .fact
    halt
.fact:
    cmp r0, 1
    beq .base
    bgt .rec
    b .base
.rec:
    sub r13, r13, 8
    st r0, [r13]
    st r15, 4[r13]
    sub r0, r0, 1
    call .fact
    ld r0, [r13]
    ld r15, 4[r13]
    mul r1, r0, r1
    add r13, r13, 8
    ret
.base:
    mov r1, 1
    ret
