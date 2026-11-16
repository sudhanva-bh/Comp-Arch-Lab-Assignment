#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_state.h"
#include "memory.h"
#include "isa_registry.h"
#include "rv32i_base.h"
#include "assembler.h"
#include "cli_display.h"
#include "pipeline_state.h"
#include "rv32f.h"
#include "rv32m.h"
#include "zicsr.h"

#define DEFAULT_MEM_SIZE   (1u << 20)   /* 1 MB */
#define DEFAULT_MAX_CYCLES 1000000ull

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s <program.s> [options]\n"
        "  --batch              run batch; FP/CSR inputs default to forward+2bit\n"
        "  --single-cycle       select original batch engine\n"
        "  --pipeline           sheet timing, conservative stalling\n"
        "  --pipeline-forward   sheet timing, forwarding and load-use detection\n"
        "  --hazard POLICY      reference timing: stall, forward, none\n"
        "  --pipeline-basic     reference timing, no hazards (teaching mode)\n"
        "  --predictor NAME     W9: not-taken, btfn, 1bit, 2bit (forward by default)\n"
        "  --trace              pipeline cycle trace to stderr\n"
        "  --max-cycles N       cycle limit for --batch mode (default %llu)\n"
        "  --mem-size N         memory size in bytes (default %u)\n",
        prog, (unsigned long long)DEFAULT_MAX_CYCLES, DEFAULT_MEM_SIZE);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *program_path = argv[1];
    int batch_mode = 0, pipeline_mode = 0, forwarding = 0;
    int reference_timing = 0, no_hazards = 0, trace = 0;
    const BranchPredictor *predictor = NULL;
    int explicit_engine = 0, explicit_hazard = 0;
    uint64_t max_cycles = DEFAULT_MAX_CYCLES;
    uint32_t mem_size = DEFAULT_MEM_SIZE;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = 1;
        } else if (strcmp(argv[i], "--single-cycle") == 0) {
            pipeline_mode = 0; batch_mode = 1; explicit_engine = 1;
        } else if (strcmp(argv[i], "--pipeline") == 0 || strcmp(argv[i], "--pipeline-forward") == 0) {
            pipeline_mode = batch_mode = 1; explicit_engine = 1;
            forwarding = strcmp(argv[i], "--pipeline-forward") == 0;
            reference_timing = no_hazards = 0;
        } else if (strcmp(argv[i], "--pipeline-basic") == 0) {
            pipeline_mode = batch_mode = reference_timing = no_hazards = 1; explicit_engine = 1;
            forwarding = 0;
        } else if (strcmp(argv[i], "--hazard") == 0 && i + 1 < argc) {
            const char *policy = argv[++i];
            if (strcmp(policy, "stall") && strcmp(policy, "forward") && strcmp(policy, "none")) {
                fprintf(stderr, "invalid hazard policy '%s'\n", policy); return 1;
            }
            pipeline_mode = batch_mode = reference_timing = 1; explicit_hazard = 1;
            forwarding = strcmp(policy, "forward") == 0;
            no_hazards = strcmp(policy, "none") == 0;
        } else if (strcmp(argv[i], "--predictor") == 0 && i + 1 < argc) {
            predictor = bp_find(argv[++i]);
            if (!predictor) {
                fprintf(stderr, "invalid predictor '%s'\n", argv[i]); return 1;
            }
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--max-cycles") == 0 && i + 1 < argc) {
            max_cycles = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--mem-size") == 0 && i + 1 < argc) {
            mem_size = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else {
            fprintf(stderr, "unrecognized option '%s'\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (predictor) {
        if (explicit_engine) {
            fprintf(stderr, "--predictor cannot be combined with an explicit legacy engine\n");
            return 1;
        }
        pipeline_mode = batch_mode = reference_timing = 1;
        if (!explicit_hazard) { forwarding = 1; no_hazards = 0; }
    }

    /* 1. Bring up the ISA. Later labs add their own register_rv32m(),
     *    register_rv32f(), register_custom_ext(), etc. right here. */
    isa_registry_init();
    register_rv32i_base();
    register_rv32m(); register_zicsr(); register_rv32f();

    /* 2. Set up memory and assemble the program into it. */
    Memory mem;
    if (memory_init(&mem, mem_size) != 0) {
        fprintf(stderr, "error: could not allocate %u bytes of simulated memory\n", mem_size);
        return 1;
    }

    uint32_t entry_point = 0;
    AsmError err;
    AsmStatus asm_status = assemble_file(program_path, &mem, &entry_point, &err);
    if (asm_status != ASM_OK) {
        fprintf(stderr, "assembly error");
        if (err.line_number > 0) fprintf(stderr, " (line %d)", err.line_number);
        fprintf(stderr, ": %s\n", err.message);
        memory_free(&mem);
        return 1;
    }

    /* FP/CSR programs use the W10 starter's default pipeline unless a legacy
       engine was explicitly selected. Integer-only defaults stay unchanged. */
    if(assembler_uses_fp() && !explicit_engine && !predictor) {
        predictor=&bp_2bit; pipeline_mode=batch_mode=reference_timing=1;
        if(!explicit_hazard)forwarding=1;
    }

    printf("assembled '%s' ok. entry point = 0x%08x\n", program_path, entry_point);

    /* 3. Reset the CPU at the program's entry point and run it. */
    CPUState cpu;
    cpu_reset(&cpu, entry_point);

    if (pipeline_mode) {
        Pipeline p;
        pipeline_init(&p, forwarding);
        p.reference_timing = reference_timing; p.no_hazards = no_hazards; p.trace = trace;
        pipeline_set_predictor(&p, predictor);
        pipeline_run_batch(&p, &cpu, &mem, max_cycles);
    } else if (batch_mode) {
        run_batch(&cpu, &mem, max_cycles);
    } else {
        run_repl(&cpu, &mem);
    }

    memory_free(&mem);
    return 0;
}
