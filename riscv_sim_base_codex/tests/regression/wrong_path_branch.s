# Only the first conditional branch executes. Neither younger control flow
# instruction may train a predictor or be counted as a resolved branch/jump.
main:
    addi a7, zero, 10
    beq zero, zero, done
    bne zero, zero, done
    jal zero, done
done:
    ecall
