#include "control.h"
#include <string.h>

ControlSignals control_decode(const DecodedInstr *i, const InstructionHandler *h) {
    (void)i;
    ControlSignals ctrl;
    memset(&ctrl, 0, sizeof(ctrl));

    if (h == NULL) {
        return ctrl;
    }

    switch (h->format) {
        case FMT_R:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_ALU;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 1;
            break;

        case FMT_I:
        case FMT_I_SHAMT:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_ALU;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 0;
            break;

        case FMT_U:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_ALU;
            ctrl.uses_rs1  = 0;
            ctrl.uses_rs2  = 0;
            break;

        case FMT_I_LOAD:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_MEM;
            ctrl.mem_read  = 1;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 0;
            break;

        case FMT_S:
            ctrl.reg_write = 0;
            ctrl.wb_src    = WB_NONE;
            ctrl.mem_write = 1;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 1;
            break;

        case FMT_B:
            ctrl.reg_write = 0;
            ctrl.wb_src    = WB_NONE;
            ctrl.is_branch = 1;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 1;
            break;

        case FMT_J:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_PC4;
            ctrl.is_jump   = 1;
            ctrl.uses_rs1  = 0;
            ctrl.uses_rs2  = 0;
            break;

        case FMT_I_JALR:
            ctrl.reg_write = 1;
            ctrl.wb_src    = WB_PC4;
            ctrl.is_jump   = 1;
            ctrl.uses_rs1  = 1;
            ctrl.uses_rs2  = 0;
            break;

        case FMT_I_SYS:
            ctrl.reg_write = 0;
            ctrl.wb_src    = WB_NONE;
            ctrl.is_system = 1;
            ctrl.uses_rs1  = 0;
            ctrl.uses_rs2  = 0;
            break;

        default:
            break;
    }

    return ctrl;
}
