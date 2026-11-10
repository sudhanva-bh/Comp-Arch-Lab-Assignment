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
#include "branch_predictor.h"

#define DEFAULT_MEM_SIZE   (1u << 20)   /* 1 MB */
#define DEFAULT_MAX_CYCLES 1000000ull

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s <program.s> [options]\n"
        "  --batch              run to completion with no interaction, then dump state\n"
        "  --single-cycle       run in single-cycle mode (reference oracle)\n"
        "  --pipeline           run on five-stage pipeline with stalling only (Lab 2)\n"
        "  --pipeline-forward   run on five-stage pipeline with data forwarding (Lab 3)\n"
        "  --predictor <name>   branch predictor: not-taken, btfn, 1bit, 2bit (Lab 4, default 2bit)\n"
        "  --hazard <mode>      hazard unit: stall or forward (default forward)\n"
        "  --trace              print cycle-by-cycle pipeline trace\n"
        "  --max-cycles N       cycle limit for batch/pipeline mode (default %llu)\n"
        "  --mem-size N         memory size in bytes (default %u)\n",
        prog, (unsigned long long)DEFAULT_MAX_CYCLES, DEFAULT_MEM_SIZE);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *program_path = argv[1];
    int batch_mode = 0;
    int pipeline_mode = 0;
    int single_cycle_forced = 0;
    int forwarding = 1; /* default for Lab 4 is forwarding */
    int trace_mode = 0;
    const char *predictor_name = NULL;
    uint64_t max_cycles = DEFAULT_MAX_CYCLES;
    uint32_t mem_size = DEFAULT_MEM_SIZE;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = 1;
        } else if (strcmp(argv[i], "--single-cycle") == 0) {
            single_cycle_forced = 1;
        } else if (strcmp(argv[i], "--pipeline") == 0) {
            pipeline_mode = 1;
            forwarding = 0;
        } else if (strcmp(argv[i], "--pipeline-forward") == 0) {
            pipeline_mode = 1;
            forwarding = 1;
        } else if (strcmp(argv[i], "--predictor") == 0 && i + 1 < argc) {
            predictor_name = argv[++i];
            pipeline_mode = 1;
        } else if (strcmp(argv[i], "--hazard") == 0 && i + 1 < argc) {
            const char *h = argv[++i];
            if (strcmp(h, "stall") == 0) forwarding = 0;
            else if (strcmp(h, "forward") == 0) forwarding = 1;
            else {
                fprintf(stderr, "unknown hazard mode '%s' (expected 'stall' or 'forward')\n", h);
                return 1;
            }
            pipeline_mode = 1;
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace_mode = 1;
            pipeline_mode = 1;
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

    if (single_cycle_forced) {
        pipeline_mode = 0;
        batch_mode = 1;
    }

    const BranchPredictor *pred = NULL;
    if (predictor_name) {
        pred = bp_find(predictor_name);
        if (!pred) {
            fprintf(stderr, "unknown predictor '%s' (options: not-taken, btfn, 1bit, 2bit)\n", predictor_name);
            return 1;
        }
    }

    /* 1. Bring up the ISA. */
    isa_registry_init();
    register_rv32i_base();

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

    printf("assembled '%s' ok. entry point = 0x%08x\n", program_path, entry_point);

    /* 3. Reset the CPU at the program's entry point and run it. */
    CPUState cpu;
    cpu_reset(&cpu, entry_point);

    if (pipeline_mode) {
        Pipeline pipe;
        pipeline_init(&pipe, forwarding, pred, trace_mode);
        pipeline_run_batch(&pipe, &cpu, &mem, max_cycles);
    } else if (batch_mode) {
        run_batch(&cpu, &mem, max_cycles);
    } else {
        run_repl(&cpu, &mem);
    }

    memory_free(&mem);
    return 0;
}
