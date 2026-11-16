CC      := gcc
CFLAGS  := -MMD -MP -std=c11 -Wall -Wextra -g -O0 \
           -Icore -Iisa -Iassembler -Iui -Ipipeline
LDFLAGS :=

BIN := rvsim

SRCS := \
	core/cpu_state.c \
	core/memory.c \
	core/isa_registry.c \
	core/decoder.c \
	core/executor.c \
	isa/rv32i_base.c \
	assembler/assembler.c \
	ui/cli_display.c \
	pipeline/branch_predictor.c \
	pipeline/bp_static.c \
	pipeline/bp_dynamic.c \
	pipeline/control.c \
	pipeline/hazard.c \
	pipeline/pipeline.c \
	main.c

OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d) tests/pipeline_checks.d tests/predictor_checks.d

-include $(DEPS)
.DEFAULT_GOAL := all

.PHONY: all clean run memcheck test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Example: make run PROG=tests/sample1.s
run: $(BIN)
	./$(BIN) $(PROG) --batch

# Requires valgrind installed. Example: make memcheck PROG=tests/sample1.s
memcheck: $(BIN)
	valgrind --error-exitcode=1 --leak-check=full ./$(BIN) $(PROG) --batch

# Regression suite checks the oracle, lab-sheet timing, supplied goldens,
# and full architectural state (including memory on faults and exit).
tests/pipeline_checks: tests/check_state.c $(filter-out main.o,$(OBJS))
	$(CC) $(CFLAGS) -o $@ $< $(filter-out main.o,$(OBJS)) $(LDFLAGS)

tests/predictor_checks: tests/check_predictor.c $(filter-out main.o,$(OBJS))
	$(CC) $(CFLAGS) -o $@ $< $(filter-out main.o,$(OBJS)) $(LDFLAGS)

test: $(BIN) tests/pipeline_checks tests/predictor_checks
	./tests/predictor_checks
	python3 tests/run_tests.py

clean:
	rm -f $(OBJS) $(DEPS) $(BIN) tests/pipeline_checks tests/predictor_checks
