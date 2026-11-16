main:
    fsflagsi zero, 1
    csrrw s0, 0x300, zero
    fdiv.s f1, f2, f3
