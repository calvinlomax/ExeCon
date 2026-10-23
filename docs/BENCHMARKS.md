# Initial benchmark

Run `scripts/benchmark.sh`. It makes a Release build and measures 200,000 repetitions of a four-instruction register-only block (800,000 guest instructions). Native and interpreter final registers, RIP and flags must match or the benchmark fails.

Observed on the development arm64 Mac, 2026-10-23, Apple clang 21 Release:

| Measure | Result |
|---|---:|
| Interpreter | 6.85 ms |
| Native code | 2.13 ms |
| Native code size | 228 bytes |
| Arithmetic-only ratio | 3.21× |
| State checksum | 3,400,000 |

This is a narrow microbenchmark, excludes translation cost, guest-memory traffic and dispatch, and is not an application-performance claim. The sample is a baseline, not a statistically rigorous benchmark campaign. Production compatibility work takes priority over optimization.

Current costs visible in the implementation: checked map lookup and byte loops for guest memory, sequentially-consistent fences, eager page-backed heap allocation, repeated register loads/stores, flag-helper calls, and interpreter dispatch for memory/control flow. Future profiling should separate decoding, translation, cache-hit dispatch, invalidation, API calls, guest memory and startup. Do not remove fences or page-generation checks until concurrency/SMC differential tests justify the change. Direct chaining, general register allocation, SSE2 and TSO-barrier optimization are not complete.
