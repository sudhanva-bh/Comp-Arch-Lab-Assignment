main:
    li t0, 0x80000000
    li t1, -1
    mul s0, t0, t1
    mulh s1, t0, t1
    mulhsu s2, t0, t1
    mulhu s3, t0, t1
    div s4, t0, t1
    rem s5, t0, t1
    divu s6, t0, zero
    remu s7, t0, zero
    div s8, t1, zero
    rem s9, t1, zero
    divu s10, t1, t0
    remu s11, t1, t0
    addi a7, zero, 10
    ecall
