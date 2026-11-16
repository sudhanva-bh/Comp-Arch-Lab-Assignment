# x0 still discards a load result; no false load-use stall.
main:
    lw zero, 256(zero)
    addi s0, zero, 7
    addi a7, zero, 10
    ecall
