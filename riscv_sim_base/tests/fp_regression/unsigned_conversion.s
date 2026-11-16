main:
    li t0, 0xffffffff
    fcvt.s.wu f0, t0, rdn
    fcvt.s.wu f1, t0, rup
    fcvt.wu.s s0, f0, rtz
    fcvt.wu.s s1, f1, rtz
    fcvt.s.w f2, t0
    fmv.s f3, f2
    fmv.x.w s2, f3
    fabs.s f4, f3
    fcvt.w.s s3, f4
    frflags s4
    fsflagsi zero, 0
    fsrmi zero, 4
    li t1, 0x40200000
    fmv.w.x f5, t1
    fcvt.w.s s5, f5
    addi a7, zero, 10
    ecall
