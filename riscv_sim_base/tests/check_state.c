/* Differential architectural checks include all memory bytes, not just the
   register dump. In particular, faults/exits must suppress younger stores. */
#include "assembler.h"
#include "executor.h"
#include "pipeline_state.h"
#include "rv32i_base.h"
#include "rv32f.h"
#include "rv32m.h"
#include "zicsr.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    isa_registry_init(); register_rv32i_base(); register_rv32m(); register_zicsr(); register_rv32f();
    Memory original;
    if (memory_init(&original, 1u << 20)) return 2;
    uint32_t entry; AsmError err;
    if (assemble_file(argv[1], &original, &entry, &err) != ASM_OK) return 2;
    Memory ref;
    if (memory_init(&ref, original.size)) return 2;
    memcpy(ref.data, original.data, ref.size);
    CPUState oracle; cpu_reset(&oracle, entry);
    uint64_t ticks = 0;
    while (oracle.status == CPU_RUNNING && ticks++ < 100000) execute_cycle(&oracle, &ref);
    if (oracle.status == CPU_RUNNING) return 2;
    for (int mode = 0; mode < 12; mode++) {
        Memory mem;
        if (memory_init(&mem, original.size)) return 2;
        memcpy(mem.data, original.data, mem.size);
        CPUState cpu; cpu_reset(&cpu, entry);
        Pipeline p; pipeline_init(&p, mode & 1); p.reference_timing = mode >= 2;
        if (mode >= 4) {
            const BranchPredictor *predictors[] = {&bp_not_taken, &bp_btfn, &bp_1bit, &bp_2bit};
            pipeline_set_predictor(&p, predictors[(mode - 4) / 2]);
        }
        while (cpu.status == CPU_RUNNING && p.cycles < 100000) pipeline_cycle(&p, &cpu, &mem);
        int bad = cpu.status != oracle.status || cpu.fcsr!=oracle.fcsr ||
                  memcmp(cpu.fregs,oracle.fregs,sizeof(cpu.fregs)) || memcmp(cpu.regs, oracle.regs, sizeof(cpu.regs)) ||
                  memcmp(mem.data, ref.data, mem.size);
        if (cpu.status == CPU_MEM_FAULT || cpu.status == CPU_ILLEGAL_INSTRUCTION)
            bad |= cpu.fault_addr != oracle.fault_addr;
        if (bad) { fprintf(stderr, "%s architectural mismatch, mode %d\n", argv[1], mode); return 1; }
        memory_free(&mem);
    }
    memory_free(&ref); memory_free(&original);
    return 0;
}
