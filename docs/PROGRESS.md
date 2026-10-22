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
