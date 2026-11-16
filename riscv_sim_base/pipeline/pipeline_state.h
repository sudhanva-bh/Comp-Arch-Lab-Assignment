#ifndef PIPELINE_STATE_H
#define PIPELINE_STATE_H
#include "control.h"
#include "branch_predictor.h"
enum { PF_NONE, PF_ILLEGAL, PF_FETCH, PF_MEM };
typedef struct { int valid; uint32_t pc, raw, pred_next; int fault; uint64_t stalled_cycles; } IFID;
typedef struct {
    int valid; uint32_t pc; int fault; uint64_t stalled_cycles;
    DecodedInstr instr; const InstructionHandler *handler; ControlSignals ctrl;
    uint32_t rs1_val, rs2_val, rs3_val, pred_next;
} IDEX;
typedef struct {
    int valid; uint32_t pc; int fault; uint64_t stalled_cycles;
    DecodedInstr instr; const InstructionHandler *handler; ControlSignals ctrl;
    uint32_t alu_result, store_val, fault_addr;
    int fcsr_write; uint32_t fcsr_val;
} EXMEM;
typedef struct {
    int valid; uint32_t pc; int fault; uint64_t stalled_cycles;
    DecodedInstr instr; const InstructionHandler *handler; ControlSignals ctrl;
    uint32_t wb_val, fault_addr, syscall_arg;
    int fcsr_write; uint32_t fcsr_val;
} MEMWB;
typedef struct {
    IFID if_id; IDEX id_ex; EXMEM ex_mem; MEMWB mem_wb;
    uint64_t cycles, retired, stalls, flushes;
    uint64_t load_use_stalls, fwd_ex, fwd_mem;
    uint64_t discarded_stalls; /* younger stalls overlapping terminal drain */
    const BranchPredictor *predictor; /* NULL preserves W7/W8 behavior */
    BPState bp;
    uint64_t branches, mispredicts, jal, jalr;
    int forwarding;
    /* Sheet mode: conservative three-latch interlock, target fetched in EX's
       redirect cycle. Reference mode: WB-to-ID reads, two-cycle redirects. */
    int reference_timing, no_hazards, trace;
} Pipeline;
void pipeline_init(Pipeline *p, int forwarding);
void pipeline_set_predictor(Pipeline *p, const BranchPredictor *predictor);
CPUStatus pipeline_cycle(Pipeline *p, CPUState *cpu, Memory *mem);
void pipeline_run_batch(Pipeline *p, CPUState *cpu, Memory *mem, uint64_t max_cycles);
#endif
