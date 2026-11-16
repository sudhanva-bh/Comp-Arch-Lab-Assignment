main:
    la gp, slot
    addi t0, zero, 2
loop:
    lw t1, 0(gp)
    addi t1, t1, 1
    sw t1, 0(gp)
    addi t0, t0, -1
    bne t0, zero, loop
    lw s0, 0(gp)
    addi a7, zero, 10
    ecall
slot:
    .word 0
