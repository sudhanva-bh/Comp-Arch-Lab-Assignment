# RV32I Simulator — implemented through W9

The current project overview is [the repository-root README](../README.md).

- [Hands-on tutorial and sample-test commands](../tutorial.md)
- [Architecture and implementation manual](../COMPREHENSIVE_LAB_GUIDE.md)
- [Lab sheets explained from first principles](../LAB_SHEETS_EXPLAINED.md)
- [W6–W8 implementation record](W6_W8_IMPLEMENTATION.md)
- [W9 implementation record](W9_IMPLEMENTATION.md)

From this directory:

```sh
make
make test
./rvsim tests/sample1_sum_loop.s --pipeline-forward
./rvsim tests/given/lab4_branch/b1_patterns.s --predictor 2bit
```

The original single-cycle engine, W7 pipeline, W8 hazards and W9 predictors are
implemented. W10 FP programs in `tests/given/lab5_fp/` are reference-only fixtures;
FP execution is not implemented and is excluded from `make test`.
