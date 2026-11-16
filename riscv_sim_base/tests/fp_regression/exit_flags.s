main:
    fsflagsi zero, 1
    addi a7, zero, 10
    ecall
    fdiv.s f1, f2, f3
    fsw f1, 256(zero)
