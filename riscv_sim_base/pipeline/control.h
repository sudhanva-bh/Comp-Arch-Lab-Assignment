#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include "isa_registry.h"

typedef enum {
    WB_NONE = 0,
    WB_ALU,
    WB_MEM,
    WB_PC4
} WbSrc;

typedef struct {
    int   reg_write;    /* does this instruction write rd?           */
    WbSrc wb_src;       /* what value gets written back?             */
    int   mem_read;     /* MEM stage performs a load                 */
    int   mem_write;    /* MEM stage performs a store                */
    int   is_branch;    /* conditional branch - EX decides          */
    int   is_jump;      /* jal / jalr - EX always redirects         */
    int   is_system;    /* ecall - handled at WB                     */
    int   uses_rs1;     /* needed by the hazard detector (Task 4)   */
    int   uses_rs2;
} ControlSignals;

ControlSignals control_decode(const DecodedInstr *i, const InstructionHandler *h);

#endif /* CONTROL_H */
