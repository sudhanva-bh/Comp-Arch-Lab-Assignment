# RV32I Simulator — implemented through W10

The current overview is [the repository-root README](../README.md).

- [Tutorial and commands for all 15 sample tests](../tutorial.md)
- [Architecture and implementation manual](../COMPREHENSIVE_LAB_GUIDE.md)
- [Lab sheets explained from first principles](../LAB_SHEETS_EXPLAINED.md)
- [W6–W8 implementation record](W6_W8_IMPLEMENTATION.md)
- [W9 implementation record](W9_IMPLEMENTATION.md)
- [W10 implementation record](W10_IMPLEMENTATION.md)

From this directory:

```sh
make
make test
./rvsim tests/sample1_sum_loop.s --pipeline-forward
./rvsim tests/given/lab4_branch/b1_patterns.s --predictor 2bit
./rvsim tests/given/lab5_fp/f3_kernel.s --single-cycle
./rvsim tests/given/lab5_fp/f3_kernel.s --batch
./rvsim tests/given/lab5_fp/f3_kernel.s --hazard stall
```

The original single-cycle engine, W7 pipeline, W8 hazards, W9 predictors and
W10 floating-point support are implemented. FP/CSR programs default to the
reference forwarding pipeline with 2bit prediction unless an engine is explicitly
chosen; integer-only defaults remain unchanged. The numerical backend is vendored
with provenance and license, so no network is needed to build.
