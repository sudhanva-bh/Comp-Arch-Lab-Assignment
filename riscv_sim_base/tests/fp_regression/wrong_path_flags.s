main:
    fsflagsi zero, 1
    beq zero, zero, done
    fdiv.s f1, f2, f3
    .word 0
done:
    addi a7, zero, 10
    ecall
