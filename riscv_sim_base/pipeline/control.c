#include "control.h"
ControlSignals control_decode(const DecodedInstr *i, const InstructionHandler *h) {
    ControlSignals c = {0};
    unsigned flags=isa_flags(h);
    switch (h->format) {
    case FMT_R4: c.uses_rs3=1; /* fall through */
    case FMT_R: c.uses_rs2 = 1; /* fall through */
    case FMT_I: case FMT_I_SHAMT:
        c.reg_write = 1; c.wb_src = WB_ALU; c.uses_rs1 = 1; break;
    case FMT_R2: c.reg_write=1; c.wb_src=WB_ALU; c.uses_rs1=1; break;
    case FMT_CSR: c.reg_write=1; c.wb_src=WB_ALU; c.uses_rs1=(i->funct3<4); break;
    case FMT_U: c.reg_write = 1; c.wb_src = WB_ALU; break;
    case FMT_I_LOAD:
        c.reg_write = 1; c.wb_src = WB_MEM; c.mem_read = 1; c.uses_rs1 = 1; break;
    case FMT_S: c.mem_write = 1; c.uses_rs1 = c.uses_rs2 = 1; break;
    case FMT_B: c.is_branch = 1; c.uses_rs1 = c.uses_rs2 = 1; break;
    case FMT_I_JALR: c.uses_rs1 = 1; /* fall through */
    case FMT_J: c.is_jump = c.reg_write = 1; c.wb_src = WB_PC4; break;
    case FMT_I_SYS: c.is_system = 1; break;
    }
    c.is_handler=(flags&IF_EXEC_HANDLER)!=0;
    c.src1=c.uses_rs1 ? i->rs1+((flags&IF_RS1_FP)?32:0) : 0;
    c.src2=c.uses_rs2 ? i->rs2+((flags&IF_RS2_FP)?32:0) : 0;
    c.src3=c.uses_rs3 ? i->rs3+32 : 0;
    c.dst=c.reg_write ? i->rd+((flags&IF_RD_FP)?32:0) : 0;
    return c;
}
