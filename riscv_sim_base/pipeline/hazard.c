#include "hazard.h"
int produces(int valid, const ControlSignals *c, uint32_t rd, uint32_t rs) {
    return valid && c->reg_write && rd != 0 && rd == rs;
}
HazardVerdict hazard_check(const Pipeline *p, const DecodedInstr *d, const ControlSignals *c) {
    HazardVerdict v = {0, 0};
    if (p->no_hazards) return v;
    uint32_t s1 = c->uses_rs1 ? d->rs1 : 0;
    uint32_t s2 = c->uses_rs2 ? d->rs2 : 0;
    if (p->forwarding) {
        v.load_use = p->id_ex.fault == PF_NONE && p->id_ex.ctrl.mem_read &&
            (produces(p->id_ex.valid, &p->id_ex.ctrl, p->id_ex.instr.rd, s1) ||
             produces(p->id_ex.valid, &p->id_ex.ctrl, p->id_ex.instr.rd, s2));
        v.stall = v.load_use;
    } else {
        v.stall =
            produces(p->id_ex.valid, &p->id_ex.ctrl, p->id_ex.instr.rd, s1) ||
            produces(p->id_ex.valid, &p->id_ex.ctrl, p->id_ex.instr.rd, s2) ||
            produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, s1) ||
            produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, s2);
        if (!p->reference_timing)
            v.stall |= produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, s1) ||
                       produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, s2);
    }
    return v;
}
static uint32_t forward_one(const Pipeline *p, uint32_t rs, uint32_t value, Operands *o) {
    /* A load's EX/MEM value is its address, never the loaded data. */
    if (p->ex_mem.fault == PF_NONE && !p->ex_mem.ctrl.mem_read &&
        produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.instr.rd, rs)) {
        o->fwd_ex++; return p->ex_mem.alu_result;
    }
    if (p->mem_wb.fault == PF_NONE &&
        produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.instr.rd, rs)) {
        o->fwd_mem++; return p->mem_wb.wb_val;
    }
    return value;
}
Operands forward_operands(const Pipeline *p, const IDEX *d) {
    Operands o = {d->rs1_val, d->rs2_val, 0, 0};
    if (!p->forwarding || p->no_hazards || d->fault) return o;
    if (d->ctrl.uses_rs1 && d->instr.rs1) o.a = forward_one(p, d->instr.rs1, o.a, &o);
    if (d->ctrl.uses_rs2 && d->instr.rs2) o.b = forward_one(p, d->instr.rs2, o.b, &o);
    return o;
}
