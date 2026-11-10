# Test dedicated to verifying the 4 load-use hazard cases from Lab 3 Section 7
.text
.globl main
main:
    li   gp, buf

    # Case 1: load followed immediately by ALU instruction using loaded value
    lw   t0, 0(gp)
    addi t1, t0, 1          # load-use stall on t0 (t1 should be 43)

    # Case 2: load followed immediately by branch using loaded value
    lw   t2, 4(gp)
    beq  t2, zero, skip     # load-use stall on t2 (t2 is 0 -> branch taken)
    addi t3, zero, 99
skip:
    addi t3, zero, 7        # t3 should be 7

    # Case 3: load followed immediately by store using loaded value as data
    lw   t4, 8(gp)
    sw   t4, 12(gp)         # load-use stall on t4 as store data

    # Case 4: load followed immediately by load using loaded value as base address
    lw   t5, 16(gp)
    lw   t6, 0(t5)          # load-use stall on t5 as address base

    addi a7, zero, 10       # exit
    ecall

.data
buf:
    .word 42                # 0(gp)
    .word 0                 # 4(gp)
    .word 123               # 8(gp)
    .word 0                 # 12(gp)
    .word target_buf        # 16(gp)
target_buf:
    .word 999

