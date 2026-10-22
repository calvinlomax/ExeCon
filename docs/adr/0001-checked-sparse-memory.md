# ADR 1: Checked sparse guest memory
Status: accepted

## Context
Apple Silicon low-address restrictions and high PE preferred bases make direct identity mapping undesirable. Correct faults and self-modifying code detection precede raw memory performance.

## Options
Reserve a huge PROT_NONE host range; sparse software pages; isolated guest VM.

## Decision
Use sparse 4 KiB software pages keyed by guest address. Every access checks bounds, permissions and the complete range before mutation. PE32 is limited to 4 GiB, PE32+ to 47 address bits. Physical allocation is capped at 256 MiB. Guest pointers never become host pointers. Per-page monotonically changing generations enable translated-code invalidation. The JIT initially accesses memory only through checked helpers/interpreter fallback.

## Consequences
No host low-address reservation, no guest-memory SIGSEGV recovery, no raw mapped guest pointers. This deliberately deviates from the suggested upfront mmap reservation, and is slower but easier to validate. Single guest thread only; seq_cst fences are conservative but do not by themselves make the page container thread-safe. Multi-threading requires serialization/atomic accesses before enabling it. Sparse pages allow high preferred bases without allocating intervening address space.
