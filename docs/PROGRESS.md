# Progress

Date: 2026-10-22 (America/Indiana/Indianapolis). This is an incremental implementation, not a completed Windows compatibility runtime.

## Environment

- Host: Darwin arm64 (Apple Silicon); eligible for native JIT tests.
- Apple clang 21.0.0, Swift 6.4, command-line tools installed.
- Initial xcodebuild probe failed because CommandLineTools was selected. Follow-up found Xcode 27.1 (27A9275) in /Applications/Xcode.app; DEVELOPER_DIR selects it without changing system settings. App builds will use it.
- CMake 4.4.3, Ninja installed.
- llvm-mingw and clang-format absent from PATH at initial check. Installed development-only clang-format 18.1.8, Unicorn 2.1.4 and Capstone 5.0.7 in ignored .dev-tools/venv. Xcode also contains clang-format.
- Shell HTTPS probe to Microsoft Learn failed DNS (curl exit 6) in sandbox. Documentation browser can reach Microsoft Learn and Apple public docs. An authorized unsandboxed pip download succeeded; network is available outside the shell sandbox.

## M0 — scaffolding

C++20 CMake project, test harness, scripts, logging, contributor rules, signing configuration and architecture skeleton. Warnings are errors. Test harness's registration list is test-only global state. Validation and later milestones follow below.

M0 validation: `scripts/build.sh` and `scripts/run-tests.sh` pass on arm64. Ad-hoc hardened runtime signing succeeded.

## M1 — PE and guest-memory foundation (partial)

Implemented bounds-checked PE32/PE32+ parsing, section loading/permissions, DIR64/HIGHLOW relocation, named/ordinal and RVA-based delay imports, exports/forwarders, TLS callback metadata and x64 runtime-function metadata. `--inspect` prints these and all directory ranges. Sparse guest pages support high bases, 32-bit limits, transactional permission checks and write generations. Image/file/metadata limits bound parser work. Tests cover malformed/truncated headers, 10,000 seeded mutations, import/export/TLS/pdata metadata, relocation, permissions and cross-page faults.

Validation: 6 unit tests pass in normal and ASan/UBSan builds. The libFuzzer source/target is present, but the selected Apple toolchain lacks libclang_rt.fuzzer_osx.a; coverage-guided fuzzing is not claimed. Deterministic mutation fuzzing ran successfully under sanitizers.

Remaining M1 work: resource tree interpretation, bound import timestamps, load-config semantics, general real-DLL resolution/loading and TLS initialization. Directory bounds are inspected, not interpreted for these features. Unsupported relocation types and subpage section alignment fail explicitly at load. Later execution startup must reject metadata it cannot honor. See ADR 2 for milestone scope.

## M2 — integer decoder and reference interpreter (partial)

Implemented a handwritten arithmetic opcode table and mode-aware decoder, 8/16/32/64-bit operands, REX/ModRM/SIB/displacements/immediates, RIP/EIP-relative addresses, FS/GS offsets, disassembly, typed register-transfer IR, constant-address LEA optimization, integer interpreter and lazy flags. Unsupported instructions, LOCK and REP forms are explicit guest illegal-instruction faults. Decoder coverage is available through `x86::coverage()` (CLI exposure follows). No SSE/x87/atomic/string instruction support yet. CPUID advertises no optional features; RDTSC diagnoses its missing timing model.

Validation: 5,144 randomized/edge-case full-state instruction comparisons against Unicorn 2.1.4 pass in 32/64-bit interpreter modes; all general registers, defined flags, RIP and 4 KiB of memory are compared. 20,000 random decoder inputs produced 9,737 supported decodes whose lengths all match Capstone 5.0.7. Oracle uncovered a real zero-count 32-bit shift bug (upper-register zero extension); fixed and regression-tested without weakening comparisons. Undefined AF/OF/CF cases are excluded only per architectural definitions. Unit and ASan/UBSan tests pass. Unicorn needs execution outside this workspace sandbox (sandbox raises SIGILL during its memory/JIT setup); the product does not depend on it.

Remaining: broader instruction coverage, block/sequence oracle tests, SSE2, strings, atomics, multiply/divide, 16-bit address forms. Test fixtures are generated from source; no third-party binaries committed.

## M3 — native ARM64 integer backend (partial)

Implemented a handwritten ARM64 encoder, fixed scratch register allocation, actual native MOV and integer ALU emission for 8/16/32/64-bit register operands, lazy flag helper, MAP_JIT arena, W^X toggling, icache flush, signed executable/test binaries, guest-PC block cache, native-PC metadata, interpreter fallback and page-generation invalidation. Memory writes terminate blocks to detect late-written/self-modifying code correctly. No global fault-handler state.

Validation: 1,024 native full-state oracle comparisons pass against Unicorn; expanded interpreter oracle passes 5,312 cases and 20,000 decoder inputs. 1,000 multi-instruction JIT/interpreter sequences, encoding golden words, cache reuse/invalidation, SMC and precise checked faults pass. Normal and ASan/UBSan unit builds pass (native JIT intentionally disabled under sanitizers). Sanitizer build initially caught unused platform fields under -Werror; corrected without relaxing warnings.

Remaining M3 requirements: SSE2, native memory/branch translation, NZCV fast flags, direct chaining, general allocation, host signal/Mach exception recovery. See ADR 3. This is a usable native integer subset, not a completed M3.

## M4 — console vertical slice (partial)

Real llvm-mingw-compiled PE32+ C guests now execute using the native JIT with interpreter fallback, or entirely with `--interp-only`. Implemented stack/TEB/PEB/process parameters/main-image loader lists, Windows command-line quoting, UTF-16 environment block, import stubs, x64 ABI argument/return handling, thin kernel32/CRT adapters over NT-style descriptor/handle services, standard input/output, Unicode console output, process exit and per-TEB last error. The C embedding API catches all C++ exceptions and separates guest exit from engine failure.

CLI exposes inspection/import reporting, trace/IR/ARM words/stats, missing-API policies, environment/options, instruction limits, logs, decoder coverage and a JIT self-check. Optional JSON status preserves full 32-bit guest exit codes and distinguishes POSIX exit-code collisions. Unknown APIs abort even under return-error when no safe return contract is known. GUI imports are diagnostic stubs; explicit return-error can return documented failure values for a small catalog.

Validation: 14 CLI integration checks pass (hello, UTF-16 surrogate-pair output, binary echo, exit 42 with JSON status, missing GUI policy, and explicit PE32 rejection), including both native and interpreter execution. Native hello executes 5 translated blocks and 7 fallback blocks. UTF conversion/invalid sequences, Windows argument quoting, API-set resolution and C API error-boundary tests pass. Full ASan/UBSan interpreter integration is also run. `--self-check` successfully executes signed MAP_JIT code.

Dev compiler: llvm-mingw 20261020 UCRT macOS universal, SHA-256 e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e (official GitHub release asset digest). Downloaded outside sandbox and verified before extraction. No third-party binary is tracked. Compiler-built guests use an explicit entry point and -nostdlib; normal CRT startup is not yet supported.

Remaining: full CRT, real DLLs, TLS initialization, SEH/C++ exceptions, signal-to-console-event dispatch, memory-level tracing and broader APIs. Load config, TLS, delay imports, managed images, DLL entry points and non-console subsystems are rejected at startup. .pdata is parsed but SEH dispatch is absent.

## M5 — SwiftUI frontend (partial validation)

Implemented native SwiftUI frontend with child-process isolation, File > Open, .exe document UTI/association, window and Dock open-event handling, drag-and-drop, console/stdin, basic streaming ANSI SGR colors, Run/Stop/Restart, per-program settings, persistent recent-run history, diagnostics viewer and a real bundled-host JIT self-check. The app links the public C API for its version, but execution stays in the separate CLI host. Display buffers are bounded.

The Xcode project and scheme are provided. Xcode 27.1 refuses to build because its license has not been accepted on this machine; no agreement was accepted on the user's behalf. `scripts/build.sh` detects uninitialized Xcode and builds the same SwiftUI sources with the installed Command Line Tools Swift compiler, targeting macOS 14 arm64, then packages and ad-hoc signs ExeCon.app and its embedded host. This fallback successfully builds with Swift warnings-as-errors. Plist/project syntax, arm64 architecture, signatures and embedded-host MAP_JIT execution validated after fixing a case-insensitive filesystem collision: the child is Contents/Helpers/execon, separate from Contents/MacOS/ExeCon. Streaming Swift console tests cover split multibyte UTF-8, split ANSI escapes, reset and invalid sequences.

Unverified: Xcode project build, interactive Finder/Dock association and manual UI interaction. Basic console handles SGR colors, not terminal cursor movement/full terminal emulation. Stop terminates the child host; guest CTRL_C_EVENT dispatch is not implemented.

## M6 — files/heap/environment foundation (partial)

Added NT path mapping (C:, Z:, configurable drives, normalized separators/dot components, ASCII case-insensitive lookup, explicit UNC rejection, extended-length prefix stripping, NUL/CON device routing), synchronous file open/read/write/seek/size/close, process-local share checks, a checked page-backed process heap, environment lookups, current directory and high-resolution timing. Win32 adapters remain thin wrappers. File sharing checks compare filesystem identity as well as paths; a test caught a case-alias bypass on this case-insensitive Mac volume and it was fixed.

Validation: 23 unit cases, 18 interpreter/native integration checks, 10 sanitizer interpreter integration checks pass. Compiled guests perform a binary file round-trip in a temporary mapped drive, heap allocation/reallocation/zero checks, environment lookup and performance-frequency query. Tests verify invalid handles, share conflicts, case variants, invalid heap pointers and Unicode/path boundaries.

Not complete: VirtualAlloc/Free/Protect/Query wrappers, custom heaps, directory enumeration, file timestamps, full Windows path/Unicode/share semantics, threads/sync/TLS/FLS, registry, locale breadth, real DLL loading, x64 SEH/C++ exceptions, Winsock. Unsupported APIs still report and abort rather than pretending success. Heap blocks currently use whole pages with a bounded monotonic address arena; reuse and allocation performance need work. Guest binaries have access to mapped host files (Z: maps to /); this is not a security sandbox.
