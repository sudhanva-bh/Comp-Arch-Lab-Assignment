.text
.globl main
main:
    addi t0, zero, 8            # counter = 8
    addi t1, zero, 0            # sum = 0
    addi t3, zero, 0            # accumulator

loop:
    andi t2, t0, 1              # alternating 0 and 1
    beq  t2, zero, is_even      # forward branch (8 executions)

is_odd:
    addi t3, t3, 1
    jal  zero, cont             # jal executed on odd iterations (4 times)

is_even:
    addi t3, t3, 2

cont:
    addi t0, t0, -1
    bne  t0, zero, loop         # backward branch (8 executions)

    addi a0, t3, 0              # return value (result = 12)
    addi a7, zero, 10           # exit
    ecall
