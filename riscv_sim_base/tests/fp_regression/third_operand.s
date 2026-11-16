main:
    la gp, data
    flw f0, 0(gp)
    fadd.s f1, f0, f0
    fmadd.s f2, f0, f1, f1
    fadd.s f2, f2, f0
    fmadd.s f3, f0, f0, f2
    fsw f3, 4(gp)
    flw f4, 4(gp)
    fcvt.w.s s0, f4
    addi x1, zero, 7
    fmv.w.x f1, x1
    fmv.x.w s1, f1
    addi a7, zero, 10
    ecall
data:
    .float 1.0, 0.0
