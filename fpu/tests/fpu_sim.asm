; FPU smoke test for the simulator. Floats live in the normal registers.
; Expected final registers are listed in run_sim_test.sh.
.main:
    mov  r1, 3
    itof r1, r1          ; r1 = 3.0   0x40400000
    mov  r2, 2
    itof r2, r2          ; r2 = 2.0   0x40000000
    fadd r3, r1, r2      ; 5.0        0x40a00000
    fsub r4, r1, r2      ; 1.0        0x3f800000
    fmul r5, r1, r2      ; 6.0        0x40c00000
    fdiv r6, r1, r2      ; 1.5        0x3fc00000
    fdiv r7, r4, r1      ; 1/3        0x3eaaaaab (rounded)
    ftoi r8, r6          ; trunc(1.5) = 1
    mov  r9, -7
    itof r9, r9          ; -7.0       0xc0e00000
    ftoi r10, r9         ; -7         0xfffffff9
    fcmp r1, r2          ; 3.0 > 2.0  -> GT
    bgt  .gt
    mov  r11, 0
    b    .next
.gt:
    mov  r11, 1          ; taken path
.next:
    fcmp r4, r4          ; 1.0 == 1.0 -> E
    beq  .eq
    mov  r12, 0
    b    .end
.eq:
    mov  r12, 1          ; taken path
.end:
    halt
