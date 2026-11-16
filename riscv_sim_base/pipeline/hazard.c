#include "hazard.h"
int produces(int valid, const ControlSignals *c, uint32_t rd, uint32_t rs) {
    return valid && c->reg_write && rd != 0 && rd == rs;
}
static int depends(int valid,const ControlSignals *producer,const ControlSignals *consumer) {
    return produces(valid,producer,producer->dst,consumer->src1) ||
           produces(valid,producer,producer->dst,consumer->src2) ||
           produces(valid,producer,producer->dst,consumer->src3);
}
HazardVerdict hazard_check(const Pipeline *p,const DecodedInstr *d,const ControlSignals *c) {
    (void)d; HazardVerdict v={0,0};
    if(p->no_hazards)return v;
    if(p->forwarding) {
        v.load_use=p->id_ex.fault==PF_NONE && p->id_ex.ctrl.mem_read &&
                   depends(p->id_ex.valid,&p->id_ex.ctrl,c);
        v.stall=v.load_use;
    } else {
        v.stall=depends(p->id_ex.valid,&p->id_ex.ctrl,c) || depends(p->ex_mem.valid,&p->ex_mem.ctrl,c);
        if(!p->reference_timing)v.stall |= depends(p->mem_wb.valid,&p->mem_wb.ctrl,c);
    }
    return v;
}
static uint32_t forward_one(const Pipeline *p, uint32_t rs, uint32_t value, Operands *o) {
    /* A load's EX/MEM value is its address, never the loaded data. */
    if (p->ex_mem.fault == PF_NONE && !p->ex_mem.ctrl.mem_read &&
        produces(p->ex_mem.valid, &p->ex_mem.ctrl, p->ex_mem.ctrl.dst, rs)) {
        o->fwd_ex++; return p->ex_mem.alu_result;
    }
    if (p->mem_wb.fault == PF_NONE &&
        produces(p->mem_wb.valid, &p->mem_wb.ctrl, p->mem_wb.ctrl.dst, rs)) {
        o->fwd_mem++; return p->mem_wb.wb_val;
    }
    return value;
}
Operands forward_operands(const Pipeline *p, const IDEX *d) {
    Operands o = {d->rs1_val, d->rs2_val, d->rs3_val, 0, 0};
    if (!p->forwarding || p->no_hazards || d->fault) return o;
    if (d->ctrl.src1) o.a = forward_one(p, d->ctrl.src1, o.a, &o);
    if (d->ctrl.src2) o.b = forward_one(p, d->ctrl.src2, o.b, &o);
    if (d->ctrl.src3) o.c = forward_one(p, d->ctrl.src3, o.c, &o);
    return o;
}
