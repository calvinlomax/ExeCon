# ExeCon architecture

The CLI hosts the C++20 engine. SwiftUI launches this host over pipes. The embedding C API has a separate lifetime and never leaks C++ exceptions. Guest x86 bytes flow through a mode-aware decoder into a typed register-transfer IR, interpreted or translated into AArch64. Every guest memory access is checked; guest addresses are not host pointers. Built-in DLL stubs are guest addresses in an engine-owned import table; dispatch checks that table instead of executing native pointers.

Layers: pe (untrusted file parsing/loading) → mem (guest permissions), x86 → ir → jit, cpu (architectural state), win32 (ABI adapters) → nt (host services) → host (platform glue). Dependencies only flow downward; the execution dispatcher wires the layers together. Module interfaces are C++ headers with ownership explicit in types.

Concurrency and TSO, cache invalidation, startup, exception policy and supported IR are documented as they are implemented. No GUI rendering is implemented.

## Clean-room references

- [Microsoft PE/COFF](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [Windows x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)
- [Intel instruction manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [Arm architecture manuals](https://developer.arm.com/documentation/ddi0487/latest/)
- [Apple JIT entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-jit)

No Wine source or third-party execution engine is used in the product.

## Implemented IR and execution

IR is a typed register-transfer representation, not SSA. Every node carries a guest PC, original bytes/length, explicit sized register/memory operands, segment/address width, sequentially-consistent ordering, and flag effects. Instruction boundaries survive optimization. The initial optimizer folds constant LEA addresses. The interpreter executes each node transactionally where faults can occur; memory accesses are preflighted before writes or register changes. Lazy flags retain operation, width, operands and result. INC/DEC materialize incoming CF. Logical AF is deterministically cleared but architecturally undefined.

## ARM64 backend and memory model

A fixed scratch allocation (x1/x2 operands, x3 result, x9 temporary, x16 call target, callee-saved x19 context) emits real ARM64 MOV/load/store/ALU instructions for register-only x86 operations. A small flags helper records lazy operands; it never interprets the arithmetic instruction. The native prologue preserves x19/LR and 16-byte stack alignment. Wider NZCV flag mapping and general register allocation are future work. Native 8/16-bit writes preserve upper bits; 32-bit writes zero-extend. Unsupported blocks run through IR. The dispatcher counts native translations, cache hits, fallback dispatches and invalidations. Direct block chaining is not implemented.

The executable arena is one 4 MiB MAP_JIT mapping per dispatcher. Emission toggles pthread_jit_write_protect_np and invalidates the instruction cache. Exhaustion falls back to IR. The page-generation cache checks every covered guest code page before reuse; stores end interpreted blocks, so writes cannot run stale subsequent instructions. Protection changes also invalidate generations. Blocks are keyed by guest PC and checked for mode.

Guest memory is currently single-threaded. All reads/writes use software permissions and seq_cst fences. Native code only touches the trusted CPU context; guest memory uses checked interpreter operations. This conservatively preserves guest order but is slow. Multi-threading, atomics and fence optimization are not implemented and are not advertised. There is no unsafe relaxed-memory switch yet.

Each native block records host offsets to guest PCs and stores the current guest PC before each translated instruction. Guest memory faults are raised at the checked IR instruction; there are no native guest-pointer accesses. Native backend bugs remain host bugs rather than being disguised as guest exceptions; see ADR 3.
