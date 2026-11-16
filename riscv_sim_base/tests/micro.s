.text
.globl main
main:
    addi t0, zero, 5
    addi t1, t0, 10
    add t2, t1, t0
    addi a7, zero, 10
    ecall
