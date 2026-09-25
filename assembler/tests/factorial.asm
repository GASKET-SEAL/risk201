.main:
    mov r0, 10
    call .factorial
    nop

.factorial:
    cmp r0, 1
    beq .return
    bgt .continue
    b .return
.continue:
    sub r14, r14, 8
    st r0, [r14]
    st r15, 4[r14]
    sub r0, r0, 1
    call .factorial
    ld r0, [r14]
    ld r15, 4[r14]
    mul r1, r0, r1
    add r14, r14, 8
    ret
.return:
    mov r1, 1
    ret