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

Each native block records host offsets to guest PCs and stores the current guest PC before each translated instruction. Guest memory faults are raised at the checked IR instruction; there are no native guest-pointer accesses. The process boundary adds the current guest instruction PC to the fault, separately from the accessed address, for CLI diagnostics. Native backend bugs remain host bugs rather than being disguised as guest exceptions; see ADR 3.

## NT and Win32 contract

Win32 adapters decode RCX/RDX/R8/R9 and arguments at RSP+40 onward, validate guest buffers, call an NT-style service, translate service errors, update TEB LastErrorValue and return via the guest stack. Native host function pointers are never put in the guest IAT. Built-in stubs reside in a checked guest executable mapping; the dispatcher recognizes only registered addresses. Tokens are execute-only: data reads fault instead of treating a function token as an imported data object. Real imported data objects remain unsupported. The NT service owns duplicated standard descriptors through reference-counted RAII file objects and a process-local handle table. Future VM/thread/registry services belong here, not in ABI wrappers.

PE metadata parsing is intentionally distinct from execution capability: `--inspect` can report a PE32 or TLS-bearing image while startup rejects unsupported semantics. Resources/bound-import/load-config directories currently have only range metadata. The default prefix is ~/Library/Application Support/ExeCon/prefix (C: maps to its drive_c); filesystem behavior is described below.

## Runtime subset

NT Services now owns paths and file objects. Share enforcement is process-local and compares existing file identity to prevent case/hard-link aliases bypassing checks. Windows creation modes map to POSIX open flags; unsupported security/overlapped/extended flags fail. The process heap keeps allocation metadata and invalidates freed pages; it is deliberately simple and page-granular. Time uses steady_clock nanoseconds with a matching 1 GHz reported frequency. Guest environment is explicitly configured, not inherited wholesale from the host. ANSI entry points use UTF-8 and GetACP reports 65001.

Paths normalize Windows components before case lookup. C: maps to prefix/drive_c, Z: to host root, additional letters to configured roots. UNC, drive-relative C:foo, unsupported devices and alternate data streams are rejected. Case lookup is ASCII-only; Unicode normalization/collation and full NT namespace behavior remain deferred. The compatibility layer is not a guest security boundary.

## 32-bit execution

The same decoder/IR/interpreter runs native guest 32-bit mode inside the arm64 host. Guest addresses and GPR writes are narrowed, stack pointers and import entries are four bytes, FS holds the TEB base, and stdcall/cdecl are distinguished by module signatures. x87 state is reserved in Context but has no execution semantics yet. No WoW64 components are loaded. PE32 bypasses allocation of the native code arena and uses checked interpretation exclusively.
