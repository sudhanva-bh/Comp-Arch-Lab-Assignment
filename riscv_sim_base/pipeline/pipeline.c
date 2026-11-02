#include "pipeline_state.h"
#include "hazard.h"
#include "decoder.h"
#include "cli_display.h"
#include <stdio.h>
#include <string.h>

void pipeline_init(Pipeline *p, int forwarding) {
    memset(p, 0, sizeof(*p));
    p->forwarding = forwarding;
}

static uint32_t alu(const DecodedInstr *i, uint32_t pc, uint32_t a, uint32_t b) {
    uint32_t opcode = i->opcode;
    uint32_t funct3 = i->funct3;
    uint32_t funct7 = i->funct7;
    int32_t imm = i->imm;

    if (opcode == 0x37) { /* lui */
        return (uint32_t)imm;
    }
    if (opcode == 0x17) { /* auipc */
        return pc + (uint32_t)imm;
    }
    if (opcode == 0x03 || opcode == 0x23) { /* loads and stores: effective address */
        return a + (uint32_t)imm;
    }
    if (opcode == 0x13) { /* I-type arithmetic */
        switch (funct3) {
            case 0x0: return a + (uint32_t)imm;                     /* addi */
            case 0x2: return ((int32_t)a < (int32_t)imm) ? 1 : 0;  /* slti */
            case 0x3: return (a < (uint32_t)imm) ? 1 : 0;          /* sltiu */
            case 0x4: return a ^ (uint32_t)imm;                     /* xori */
            case 0x6: return a | (uint32_t)imm;                     /* ori */
            case 0x7: return a & (uint32_t)imm;                     /* andi */
            case 0x1: return a << (imm & 0x1F);                     /* slli */
            case 0x5:
                if (funct7 & 0x20) {
                    return (uint32_t)((int32_t)a >> (imm & 0x1F)); /* srai */
                } else {
                    return a >> (imm & 0x1F);                      /* srli */
                }
            default: return 0;
        }
    }
    if (opcode == 0x33) { /* R-type arithmetic */
        uint32_t shamt = b & 0x1F;
        switch (funct3) {
            case 0x0:
                if (funct7 & 0x20) {
                    return a - b;                                  /* sub */
                } else {
                    return a + b;                                  /* add */
                }
            case 0x1: return a << shamt;                            /* sll */
            case 0x2: return ((int32_t)a < (int32_t)b) ? 1 : 0;    /* slt */
            case 0x3: return (a < b) ? 1 : 0;                      /* sltu */
            case 0x4: return a ^ b;                                 /* xor */
            case 0x5:
                if (funct7 & 0x20) {
                    return (uint32_t)((int32_t)a >> shamt);        /* sra */
                } else {
                    return a >> shamt;                             /* srl */
                }
            case 0x6: return a | b;                                 /* or */
            case 0x7: return a & b;                                 /* and */
            default: return 0;
        }
    }
    return 0;
}

static int branch_taken(uint32_t funct3, uint32_t a, uint32_t b) {
    switch (funct3) {
        case 0x0: return a == b;                          /* beq */
        case 0x1: return a != b;                          /* bne */
        case 0x4: return (int32_t)a < (int32_t)b;         /* blt */
        case 0x5: return (int32_t)a >= (int32_t)b;        /* bge */
        case 0x6: return a < b;                           /* bltu */
        case 0x7: return a >= b;                          /* bgeu */
        default: return 0;
    }
}

CPUStatus pipeline_cycle(Pipeline *p, CPUState *cpu, Memory *mem) {
    Pipeline nx = *p;
    nx.cycles = p->cycles + 1;

    int stall = 0;
    int flush = 0;
    uint32_t redirect_pc = 0;

    /* -------------------------------------------------------------
     * 1. WB Stage (reads p->mem_wb)
     * ------------------------------------------------------------- */
    if (p->mem_wb.valid) {
        if (p->mem_wb.fault != PF_NONE) {
            cpu->status = (p->mem_wb.fault == PF_ILLEGAL) ? CPU_ILLEGAL_INSTRUCTION : CPU_MEM_FAULT;
            cpu->fault_addr = p->mem_wb.fault_addr;
            if (p->mem_wb.fault == PF_ILLEGAL) {
                snprintf(cpu->fault_msg, sizeof(cpu->fault_msg),
                         "illegal instruction 0x%08x at pc=0x%08x",
                         p->mem_wb.instr.raw, p->mem_wb.pc);
            } else {
                snprintf(cpu->fault_msg, sizeof(cpu->fault_msg),
                         "memory fault at addr=0x%08x pc=0x%08x",
                         p->mem_wb.fault_addr, p->mem_wb.pc);
            }
            *p = nx;
            cpu->cycle_count = p->cycles;
            return cpu->status;
        }

        if (p->mem_wb.ctrl.reg_write) {
            cpu_write_reg(cpu, p->mem_wb.instr.rd, p->mem_wb.wb_val);
        }

        if (p->mem_wb.ctrl.is_system) {
            uint32_t saved_pc = cpu->pc;
            cpu->pc = p->mem_wb.pc;
            p->mem_wb.handler->execute(cpu, mem, &p->mem_wb.instr);
            cpu->pc = saved_pc;
            if (cpu->status != CPU_RUNNING) {
                nx.retired++;
                *p = nx;
                cpu->cycle_count = p->cycles;
                return cpu->status;
            }
        }

        nx.retired++;
    }

    /* -------------------------------------------------------------
     * 2. MEM Stage (reads p->ex_mem, writes nx.mem_wb)
     * ------------------------------------------------------------- */
    nx.mem_wb.valid = 0;
    if (p->ex_mem.valid) {
        nx.mem_wb.valid = 1;
        nx.mem_wb.pc = p->ex_mem.pc;
        nx.mem_wb.fault = p->ex_mem.fault;
        nx.mem_wb.instr = p->ex_mem.instr;
        nx.mem_wb.handler = p->ex_mem.handler;
        nx.mem_wb.ctrl = p->ex_mem.ctrl;
        nx.mem_wb.fault_addr = p->ex_mem.fault_addr;
        nx.mem_wb.wb_val = p->ex_mem.alu_result;

        if (p->ex_mem.fault == PF_NONE) {
            if (p->ex_mem.ctrl.mem_read) {
                uint32_t addr = p->ex_mem.alu_result;
                MemStatus ms = MEM_OK;
                switch (p->ex_mem.instr.funct3) {
                    case 0x0: { /* lb: sign-extend */
                        uint8_t b = 0;
                        ms = memory_read_byte(mem, addr, &b);
                        nx.mem_wb.wb_val = (uint32_t)(int32_t)(int8_t)b;
                        break;
                    }
                    case 0x1: { /* lh: sign-extend */
                        uint16_t h = 0;
                        ms = memory_read_half(mem, addr, &h);
                        nx.mem_wb.wb_val = (uint32_t)(int32_t)(int16_t)h;
                        break;
                    }
                    case 0x2: { /* lw */
                        uint32_t w = 0;
                        ms = memory_read_word(mem, addr, &w);
                        nx.mem_wb.wb_val = w;
                        break;
                    }
                    case 0x4: { /* lbu: zero-extend */
                        uint8_t b = 0;
                        ms = memory_read_byte(mem, addr, &b);
                        nx.mem_wb.wb_val = (uint32_t)b;
                        break;
                    }
                    case 0x5: { /* lhu: zero-extend */
                        uint16_t h = 0;
                        ms = memory_read_half(mem, addr, &h);
                        nx.mem_wb.wb_val = (uint32_t)h;
                        break;
                    }
                    default:
                        break;
                }
                if (ms != MEM_OK) {
                    nx.mem_wb.fault = PF_MEM;
                    nx.mem_wb.fault_addr = addr;
                }
            } else if (p->ex_mem.ctrl.mem_write) {
                uint32_t addr = p->ex_mem.alu_result;
                MemStatus ms = MEM_OK;
                switch (p->ex_mem.instr.funct3) {
                    case 0x0: /* sb */
                        ms = memory_write_byte(mem, addr, (uint8_t)p->ex_mem.store_val);
                        break;
                    case 0x1: /* sh */
                        ms = memory_write_half(mem, addr, (uint16_t)p->ex_mem.store_val);
                        break;
                    case 0x2: /* sw */
                        ms = memory_write_word(mem, addr, p->ex_mem.store_val);
                        break;
                    default:
                        break;
                }
                if (ms != MEM_OK) {
                    nx.mem_wb.fault = PF_MEM;
                    nx.mem_wb.fault_addr = addr;
                }
            }
        }
    }

    /* -------------------------------------------------------------
     * 3. EX Stage (reads p->id_ex, writes nx.ex_mem)
     * ------------------------------------------------------------- */
    nx.ex_mem.valid = 0;
    if (p->id_ex.valid) {
        nx.ex_mem.valid = 1;
        nx.ex_mem.pc = p->id_ex.pc;
        nx.ex_mem.fault = p->id_ex.fault;
        nx.ex_mem.instr = p->id_ex.instr;
        nx.ex_mem.handler = p->id_ex.handler;
        nx.ex_mem.ctrl = p->id_ex.ctrl;
        nx.ex_mem.fault_addr = 0;
        nx.ex_mem.alu_result = 0;
        nx.ex_mem.store_val = p->id_ex.rs2_val;

        if (p->id_ex.fault == PF_NONE) {
            Operands o = forward_operands(p, &p->id_ex);
            nx.fwd_ex += o.fwd_ex;
            nx.fwd_mem += o.fwd_mem;
            nx.ex_mem.store_val = o.b;

            if (p->id_ex.ctrl.is_branch) {
                if (branch_taken(p->id_ex.instr.funct3, o.a, o.b)) {
                    flush = 1;
                    redirect_pc = p->id_ex.pc + (uint32_t)p->id_ex.instr.imm;
                }
            } else if (p->id_ex.ctrl.is_jump) {
                nx.ex_mem.alu_result = p->id_ex.pc + 4;
                flush = 1;
                if (p->id_ex.instr.opcode == 0x67) { /* jalr */
                    redirect_pc = (o.a + (uint32_t)p->id_ex.instr.imm) & ~1u;
                } else { /* jal */
                    redirect_pc = p->id_ex.pc + (uint32_t)p->id_ex.instr.imm;
                }
            } else {
                nx.ex_mem.alu_result = alu(&p->id_ex.instr, p->id_ex.pc, o.a, o.b);
            }
        }
    }

    /* -------------------------------------------------------------
     * 4. ID Stage (reads p->if_id, writes nx.id_ex)
     * ------------------------------------------------------------- */
    nx.id_ex.valid = 0;
    if (flush) {
        /* Branch/jump taken in EX: squash whatever is in IF/ID */
        nx.id_ex.valid = 0;
    } else if (p->if_id.valid) {
        DecodedInstr d;
        const InstructionHandler *h = NULL;
        DecodeStatus ds = decode_instruction(p->if_id.raw, &d, &h);
        ControlSignals ctrl = control_decode(&d, h);

        HazardVerdict hz = hazard_check(p, &d, &ctrl);
        if (hz.stall) {
            nx.id_ex.valid = 0;        /* 1. a bubble enters EX this cycle */
            nx.if_id = p->if_id;        /* 2. IF/ID is frozen: re-decoded next cycle */
            stall = 1;                 /* 3. local flag: IF must not fetch */
            nx.stalls++;
            if (hz.load_use) {
                nx.load_use_stalls++;
            }
        } else {
            nx.id_ex.valid = 1;
            nx.id_ex.pc = p->if_id.pc;
            nx.id_ex.fault = p->if_id.fault;
            if (nx.id_ex.fault == PF_NONE && ds == DECODE_UNKNOWN_INSTRUCTION) {
                nx.id_ex.fault = PF_ILLEGAL;
            }
            nx.id_ex.instr = d;
            nx.id_ex.handler = h;
            nx.id_ex.ctrl = ctrl;
            nx.id_ex.rs1_val = cpu_read_reg(cpu, d.rs1);
            nx.id_ex.rs2_val = cpu_read_reg(cpu, d.rs2);
        }
    }

    /* -------------------------------------------------------------
     * 5. IF Stage (writes nx.if_id)
     * ------------------------------------------------------------- */
    if (flush) {
        nx.flushes++;
        uint32_t raw = 0;
        nx.if_id.valid = 1;
        nx.if_id.pc = redirect_pc;
        if (memory_read_word(mem, redirect_pc, &raw) != MEM_OK) {
            nx.if_id.fault = PF_FETCH;
            nx.if_id.raw = 0;
        } else {
            nx.if_id.fault = PF_NONE;
            nx.if_id.raw = raw;
        }
        cpu->pc = redirect_pc + 4;
    } else if (stall) {
        /* IF is frozen: nx.if_id is already p->if_id from ID, cpu->pc unchanged */
    } else {
        uint32_t fetch_pc = cpu->pc;
        uint32_t raw = 0;
        nx.if_id.valid = 1;
        nx.if_id.pc = fetch_pc;
        if (memory_read_word(mem, fetch_pc, &raw) != MEM_OK) {
            nx.if_id.fault = PF_FETCH;
            nx.if_id.raw = 0;
        } else {
            nx.if_id.fault = PF_NONE;
            nx.if_id.raw = raw;
        }
        cpu->pc = fetch_pc + 4;
    }

    /* Commit */
    *p = nx;
    cpu->cycle_count = p->cycles;
    return cpu->status;
}

void pipeline_run_batch(Pipeline *p, CPUState *cpu, Memory *mem, uint64_t max_cycles) {
    while (cpu->status == CPU_RUNNING && p->cycles < max_cycles) {
        pipeline_cycle(p, cpu, mem);
    }
    printf("\n");
    print_registers(cpu);
    print_status(cpu);
    if (cpu->status == CPU_RUNNING) {
        printf("  -> stopped: reached max cycle limit (%llu)\n", (unsigned long long)max_cycles);
    }
    printf("pipeline: mode=%s cycles=%llu retired=%llu stalls=%llu flushes=%llu "
           "CPI=%.3f fwd_ex=%llu fwd_mem=%llu load_use=%llu\n",
           p->forwarding ? "forward" : "stall",
           (unsigned long long)p->cycles,
           (unsigned long long)p->retired,
           (unsigned long long)p->stalls,
           (unsigned long long)p->flushes,
           p->retired > 0 ? (double)p->cycles / (double)p->retired : 0.0,
           (unsigned long long)p->fwd_ex,
           (unsigned long long)p->fwd_mem,
           (unsigned long long)p->load_use_stalls);
}
