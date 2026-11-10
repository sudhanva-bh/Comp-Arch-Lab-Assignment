.text
.globl main
main:
    addi t0, zero, 4            # outer = 4
    addi t2, zero, 0            # sum = 0

outer:
    addi t1, zero, 3            # inner = 3

inner:
    addi t2, t2, 1
    addi t1, t1, -1
    bne  t1, zero, inner        # inner branch (3 iter each)

    addi t0, t0, -1
    bne  t0, zero, outer        # outer branch (4 iter)

    addi a7, zero, 10           # exit
    ecall

