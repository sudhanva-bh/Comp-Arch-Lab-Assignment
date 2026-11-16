# Exit in WB must suppress the younger store in MEM in the same cycle.
main:
    addi s0, zero, 7
    addi a7, zero, 10
    ecall
    sw s0, 256(zero)
    addi s1, zero, 99
