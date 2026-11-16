main:
    fsflagsi zero, 1
    flw f0, -1(zero)
    fdiv.s f1, f2, f3
    fsw f1, 256(zero)
