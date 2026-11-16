# The branch redirect must win over a younger dependency / illegal decode.
main:
    addi t0, zero, 7
    beq zero, zero, good
    addi t1, t0, 99
    .word 0
 good:
    addi a7, zero, 10
    ecall
