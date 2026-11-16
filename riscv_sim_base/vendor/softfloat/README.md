# Berkeley SoftFloat binary32 subset

Vendored from https://github.com/ucb-bar/berkeley-softfloat-3 at commit
07ebd29db0cfb55723a79fbbffe99103ef38e305 (Release 3e plus upstream changes).
Source/header copyright notices and the BSD-style LICENSE.txt are retained.
Only binary32 arithmetic, comparison, integer conversion and required primitives
are built. RISCV specialization supplies canonical NaNs and conversion clipping.
The source files are unmodified. platform.h is local, using portable C integer
primitives and C11 thread-local state. No network is needed to build the project.
