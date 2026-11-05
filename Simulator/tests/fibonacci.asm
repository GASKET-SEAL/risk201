# Fibonacci(10). Expect r2=55 (6-stage: 103 cycles / 76 instr / 11 flushes / 0 stalls; 4-stage: 90 cycles)

    mov r1, 10      # N (countdown)
    mov r2, 0       # a (Fib n-2)
    mov r3, 1       # b (Fib n-1)

loop:
    cmp r1, 0       # Check if countdown is 0
    beq end         # If 0, branch to end

    add r4, r2, r3  # Calculate next Fibonacci number
    mov r2, r3      # Shift 'a' to 'b'
    mov r3, r4      # Shift 'b' to 'next'

    sub r1, r1, 1   # Decrement countdown
    b loop          # Branch back to start

end:
    halt            # Stop execution