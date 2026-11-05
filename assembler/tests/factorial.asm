.main:
    mov r0, 10
    call .factorial
    halt

.factorial:
    cmp r0, 1
    beq .return
    bgt .continue
    b .return
.continue:
    sub r13, r13, 8
    st r0, [r13]
    st r15, 4[r13]
    sub r0, r0, 1
    call .factorial
    ld r0, [r13]
    ld r15, 4[r13]
    mul r1, r0, r1
    add r13, r13, 8
    ret
.return:
    mov r1, 1
    ret