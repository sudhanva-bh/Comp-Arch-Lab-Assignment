main:
    li t0, 0x61
    csrrw s0, fcsr, t0
    csrrs s1, fflags, zero
    csrrsi s2, fflags, 8
    csrrci s3, fflags, 1
    csrrwi s4, frm, 4
    csrrc s5, fflags, t0
    frcsr s6
    la gp, data
    flw f0, 0(gp)
    fsrmi zero, 2
    fcvt.w.s s7, f0
    frrm s8
    fsflagsi zero, 31
    frflags s9
    fsflags zero, zero
    frcsr s10
    addi a7, zero, 10
    ecall
data:
    .float -2.5
