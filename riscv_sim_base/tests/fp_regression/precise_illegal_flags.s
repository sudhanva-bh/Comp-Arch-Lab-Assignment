main:
    fsflagsi zero, 1
    .word 0
    fdiv.s f1, f2, f3
    fsw f1, 256(zero)
