.text
.globl main
main:
    lui     sp, 0x10
    sw      x0, 0(sp)
    addi    s0, x0, 0
loop1:
    addi    t0, s0, -2
    beq     t0, x0, br1_skip
    addi    t1, x0, 1
br1_skip:
    addi    s0, s0, 1
    addi    t2, s0, -10
    beq     t2, x0, loop1_exit
    jal     zero, loop1
loop1_exit:
    jal     zero, jump_0
jump_0:
    jal     zero, jump_1
jump_1:
    jal     zero, jump_2
jump_2:
    jal     zero, jump_3
jump_3:
    jal     zero, jump_4
jump_4:
    jal     zero, jump_5
jump_5:
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_load_use
    jal     ra, leaf_empty
    jal     ra, leaf_empty
    jal     ra, leaf_empty
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    a7, zero, 10
    ecall
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
    addi    x0, x0, 0
leaf_load_use:
    lw      t0, 0(sp)
    add     a0, a0, t0
    jalr    zero, 0(ra)
leaf_empty:
    jalr    zero, 0(ra)