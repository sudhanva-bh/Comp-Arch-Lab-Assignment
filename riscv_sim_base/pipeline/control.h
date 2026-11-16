#ifndef PIPELINE_CONTROL_H
#define PIPELINE_CONTROL_H
#include "isa_registry.h"
typedef enum { WB_NONE = 0, WB_ALU, WB_MEM, WB_PC4 } WbSrc;
typedef struct {
    int reg_write;
    WbSrc wb_src;
    int mem_read, mem_write, is_branch, is_jump, is_system;
    int uses_rs1, uses_rs2, uses_rs3, is_handler;
    unsigned src1, src2, src3, dst; /* x0=0, x1..31=1..31, f0..31=32..63 */
} ControlSignals;
ControlSignals control_decode(const DecodedInstr *i, const InstructionHandler *h);
#endif
