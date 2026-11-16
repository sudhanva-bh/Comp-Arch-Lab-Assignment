#include "pipeline_state.h"
#include "hazard.h"
#include "decoder.h"
#include "cli_display.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

void pipeline_init(Pipeline *p, int forwarding) {
    memset(p, 0, sizeof(*p));
    p->forwarding = forwarding;
}

void pipeline_set_predictor(Pipeline *p, const BranchPredictor *predictor) {
    p->predictor = predictor;
    if (predictor) {
        predictor->reset(&p->bp);
        p->reference_timing = 1;
    }
}

static uint32_t alu(const DecodedInstr *i, uint32_t pc, uint32_t a, uint32_t b) {
    if (i->opcode == 0x37) return (uint32_t)i->imm;
    if (i->opcode == 0x17) return pc + (uint32_t)i->imm;
    if (i->opcode == 0x03 || i->opcode == 0x23) return a + (uint32_t)i->imm;
    int is_r = i->opcode == 0x33;
    if (!is_r) b = (uint32_t)i->imm;
    switch (i->funct3) {
    case 0: return is_r && i->funct7 == 0x20 ? a - b : a + b;
    case 1: return a << (b & 31);
    case 2: return (int32_t)a < (int32_t)b;
    case 3: return a < b;
    case 4: return a ^ b;
    case 5: return i->funct7 == 0x20 ? (uint32_t)((int32_t)a >> (b & 31)) : a >> (b & 31);
    case 6: return a | b;
    case 7: return a & b;
    }
    return 0;
}
static int branch_taken(uint32_t f3, uint32_t a, uint32_t b) {
    switch (f3) {
    case 0: return a == b;
    case 1: return a != b;
    case 4: return (int32_t)a < (int32_t)b;
    case 5: return (int32_t)a >= (int32_t)b;
    case 6: return a < b;
    case 7: return a >= b;
    }
    return 0;
}
static MemStatus load_value(Memory *m, uint32_t addr, uint32_t f3, uint32_t *out) {
    uint8_t b = 0; uint16_t h = 0;
    MemStatus s;
    switch (f3) {
    case 0: case 4:
        s = memory_read_byte(m, addr, &b);
        *out = f3 == 0 ? (uint32_t)(int32_t)(int8_t)b : b; return s;
    case 1: case 5:
        s = memory_read_half(m, addr, &h);
        *out = f3 == 1 ? (uint32_t)(int32_t)(int16_t)h : h; return s;
    default: return memory_read_word(m, addr, out);
    }
}
static MemStatus store_value(Memory *m, uint32_t addr, uint32_t f3, uint32_t value) {
    switch (f3) {
    case 0: return memory_write_byte(m, addr, (uint8_t)value);
    case 1: return memory_write_half(m, addr, (uint16_t)value);
    default: return memory_write_word(m, addr, value);
    }
}
static void trace_cycle(const Pipeline *p, const Pipeline *nx, uint32_t fetch_pc,
                        int stall, int flush) {
    fprintf(stderr, "[cycle %" PRIu64 "] IF %08x | ID %c%08x | EX %c%08x | MEM %c%08x | WB %c%08x%s%s\n",
            nx->cycles, fetch_pc,
            p->if_id.valid ? '*' : '-', p->if_id.pc,
            p->id_ex.valid ? '*' : '-', p->id_ex.pc,
            p->ex_mem.valid ? '*' : '-', p->ex_mem.pc,
            p->mem_wb.valid ? '*' : '-', p->mem_wb.pc,
            stall ? " STALL" : "", flush ? " FLUSH" : "");
}
CPUStatus pipeline_cycle(Pipeline *p, CPUState *cpu, Memory *mem) {
    if (cpu->status != CPU_RUNNING) return cpu->status;
    Pipeline nx = *p;
    nx.cycles++;
    cpu->cycle_count = nx.cycles;
    nx.if_id.valid = nx.id_ex.valid = nx.ex_mem.valid = nx.mem_wb.valid = 0;
    int stall = 0, flush = 0;
    uint32_t redirect_pc = 0, fetch_pc = cpu->pc;

    /* WB precedes every younger side effect, including MEM stores. An exit
       or precise fault must stop the cycle immediately. */
    if (p->mem_wb.valid) {
        const MEMWB *w = &p->mem_wb;
        if (w->fault) {
            cpu->status = w->fault == PF_ILLEGAL ? CPU_ILLEGAL_INSTRUCTION : CPU_MEM_FAULT;
            cpu->fault_addr = w->fault_addr;
            snprintf(cpu->fault_msg, sizeof(cpu->fault_msg),
                     "%s at address 0x%08x (instruction pc=0x%08x)",
                     w->fault == PF_ILLEGAL ? "illegal instruction" :
                     w->fault == PF_FETCH ? "instruction fetch out of bounds" : "memory access out of bounds",
                     w->fault_addr, w->pc);
        } else {
            if (w->ctrl.reg_write) cpu_write_reg(cpu, w->instr.rd, w->wb_val);
            if (w->ctrl.is_system) {
                CPUState call = *cpu;
                call.pc = w->pc;
                if (p->reference_timing) {
                    /* Later starter models syscall arguments as ordinary
                       latched/forwarded operands, even in unsafe none mode. */
                    cpu_write_reg(&call, 17, w->wb_val);
                    cpu_write_reg(&call, 10, w->syscall_arg);
                }
                DecodedInstr instr = w->instr;
                w->handler->execute(&call, mem, &instr);
                cpu->status = call.status;
                cpu->fault_addr = call.fault_addr;
                memcpy(cpu->fault_msg, call.fault_msg, sizeof(cpu->fault_msg));
            }
            nx.retired++;
        }
        if (cpu->status != CPU_RUNNING) {
            /* A younger ID stall can overlap the final drain cycles and
               therefore need not delay this terminal instruction at all. */
            nx.discarded_stalls =
                (p->if_id.valid ? p->if_id.stalled_cycles : 0) +
                (p->id_ex.valid ? p->id_ex.stalled_cycles : 0) +
                (p->ex_mem.valid ? p->ex_mem.stalled_cycles : 0);
            if (p->trace) trace_cycle(p, &nx, fetch_pc, 0, 0);
            *p = nx; return cpu->status;
        }
    }
    if (p->ex_mem.valid) {
        const EXMEM *e = &p->ex_mem;
        MEMWB w = {0};
        w.valid = 1; w.pc = e->pc; w.stalled_cycles = e->stalled_cycles; w.fault = e->fault; w.fault_addr = e->fault_addr;
        w.instr = e->instr; w.handler = e->handler; w.ctrl = e->ctrl;
        w.wb_val = e->alu_result; w.syscall_arg = e->store_val;
        if (!w.fault) {
            MemStatus s = MEM_OK;
            if (e->ctrl.mem_read) s = load_value(mem, e->alu_result, e->instr.funct3, &w.wb_val);
            if (e->ctrl.mem_write) s = store_value(mem, e->alu_result, e->instr.funct3, e->store_val);
            if (s != MEM_OK) { w.fault = PF_MEM; w.fault_addr = e->alu_result; }
        }
        nx.mem_wb = w;
    }
    if (p->id_ex.valid) {
        const IDEX *d = &p->id_ex;
        EXMEM e = {0};
        e.valid = 1; e.pc = d->pc; e.stalled_cycles = d->stalled_cycles; e.fault = d->fault; e.fault_addr = d->pc;
        e.instr = d->instr; e.handler = d->handler; e.ctrl = d->ctrl;
        if (!d->fault) {
            Operands o = forward_operands(p, d);
            nx.fwd_ex += o.fwd_ex; nx.fwd_mem += o.fwd_mem;
            e.store_val = o.b;
            if (d->ctrl.is_branch) {
                int taken = branch_taken(d->instr.funct3, o.a, o.b);
                uint32_t target = d->pc + (uint32_t)d->instr.imm;
                redirect_pc = taken ? target : d->pc + 4;
                if (p->predictor) {
                    nx.branches++;
                    flush = redirect_pc != d->pred_next;
                    nx.mispredicts += flush;
                    p->predictor->update(&nx.bp, d->pc, target, taken);
                } else {
                    flush = taken;
                }
            } else if (d->ctrl.is_jump) {
                e.alu_result = d->pc + 4;
                flush = 1;
                redirect_pc = d->instr.opcode == 0x67 ?
                    (o.a + (uint32_t)d->instr.imm) & ~1u : d->pc + (uint32_t)d->instr.imm;
                if (p->predictor) {
                    if (d->instr.opcode == 0x67) {
                        nx.jalr++; /* No indirect predictor: always redirect. */
                    } else {
                        nx.jal++;
                        flush = redirect_pc != d->pred_next;
                    }
                }
            } else if (d->ctrl.is_system) {
                e.alu_result = o.a;
            } else {
                e.alu_result = alu(&d->instr, d->pc, o.a, o.b);
            }
        }
        nx.ex_mem = e;
    }
    /* A redirect beats a younger hazard or decode fault. */
    if (flush) {
        nx.flushes++;
        cpu->pc = redirect_pc;
    } else if (p->if_id.valid) {
        IDEX d = {0};
        d.valid = 1; d.pc = p->if_id.pc; d.stalled_cycles = p->if_id.stalled_cycles; d.fault = p->if_id.fault;
        d.pred_next = p->if_id.pred_next;
        if (!d.fault && decode_instruction(p->if_id.raw, &d.instr, &d.handler) != DECODE_OK)
            d.fault = PF_ILLEGAL;
        if (!d.fault) {
            d.ctrl = control_decode(&d.instr, d.handler);
            if (d.ctrl.is_system && p->reference_timing) {
                d.ctrl.uses_rs1 = d.ctrl.uses_rs2 = 1;
                d.instr.rs1 = 17; d.instr.rs2 = 10;
            }
            HazardVerdict hz = hazard_check(p, &d.instr, &d.ctrl);
            if (hz.stall) {
                stall = 1; nx.stalls++; nx.load_use_stalls += hz.load_use;
                nx.if_id = p->if_id; nx.if_id.stalled_cycles++;
            } else {
                d.rs1_val = cpu_read_reg(cpu, d.instr.rs1);
                d.rs2_val = cpu_read_reg(cpu, d.instr.rs2);
            }
        }
        if (!stall) nx.id_ex = d;
    }
    if (!stall && !(flush && p->reference_timing)) {
        IFID f = {0}; f.valid = 1; f.pc = cpu->pc;
        fetch_pc = f.pc;
        if (memory_read_word(mem, f.pc, &f.raw) != MEM_OK) f.fault = PF_FETCH;
        f.pred_next = f.pc + 4;
        if (p->predictor && !f.fault) {
            DecodedInstr instr;
            const InstructionHandler *handler;
            /* Reuse the decoder for target immediates; IF never reads registers. */
            if (decode_instruction(f.raw, &instr, &handler) == DECODE_OK) {
                uint32_t target = f.pc + (uint32_t)instr.imm;
                if (handler->format == FMT_B &&
                    p->predictor->predict(&nx.bp, f.pc, target)) f.pred_next = target;
                else if (handler->format == FMT_J) f.pred_next = target;
            }
        }
        nx.if_id = f;
        cpu->pc = f.pred_next;
    }
    if (p->trace) trace_cycle(p, &nx, fetch_pc, stall, flush);
    *p = nx;
    return cpu->status;
}
void pipeline_run_batch(Pipeline *p, CPUState *cpu, Memory *mem, uint64_t max_cycles) {
    while (cpu->status == CPU_RUNNING && p->cycles < max_cycles) pipeline_cycle(p, cpu, mem);
    putchar('\n');
    print_registers(cpu); print_status(cpu);
    printf("pipeline: mode=%s timing=%s cycles=%" PRIu64 " retired=%" PRIu64
           " stalls=%" PRIu64 " flushes=%" PRIu64 " CPI=%.3f fwd_ex=%" PRIu64
           " fwd_mem=%" PRIu64 " load_use=%" PRIu64 " discarded_stalls=%" PRIu64 "\n",
           p->no_hazards ? "none" : p->forwarding ? "forward" : "stall",
           p->reference_timing ? "reference" : "sheet", p->cycles, p->retired, p->stalls, p->flushes,
           p->retired ? (double)p->cycles / p->retired : 0.0,
           p->fwd_ex, p->fwd_mem, p->load_use_stalls, p->discarded_stalls);
    if (p->predictor) {
        printf("predictor: name=%s branches=%" PRIu64 " mispredicts=%" PRIu64
               " accuracy=%.1f jal=%" PRIu64 " jalr=%" PRIu64 " redirects=%" PRIu64
               " squashed=%" PRIu64 "\n", p->predictor->name, p->branches, p->mispredicts,
               p->branches ? 100.0 * (p->branches - p->mispredicts) / p->branches : 0.0,
               p->jal, p->jalr, p->flushes, 2 * p->flushes);
    }
    if (cpu->status == CPU_RUNNING)
        printf("  -> stopped: reached max cycle limit (%" PRIu64 ")\n", max_cycles);
}
