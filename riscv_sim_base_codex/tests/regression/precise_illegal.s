# The older write retires; the younger store and register write must not.
main:
    addi s0, zero, 7
    .word 0
    sw s0, 256(zero)
    addi s1, zero, 99
