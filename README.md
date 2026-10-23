# ExeCon

ExeCon is an experimental, clean-room Windows PE console runner for Apple Silicon. It uses a handwritten x86 decoder, a reference IR interpreter, a small native ARM64 translator and a custom Win32 compatibility layer. The native SwiftUI app launches the CLI in a separate process.

**Working today:** the included compiler-built PE32+ and PE32 integer console tests, binary I/O, Unicode console output, a small CRT subset, files, process heap, environment and TEB/PEB startup. PE32+ uses native register-only blocks with interpreter fallback; PE32 currently uses the interpreter.

**Not complete:** general Windows compatibility, SSE2/x87, full CRT startup, threads, SEH/C++ exceptions, real DLL loading, registry, Winsock or Windows GUI rendering. **7-Zip and curl do not pass acceptance.** HTTPS through Schannel/SSPI is out of scope. See [compatibility](docs/COMPATIBILITY.md), [progress](docs/PROGRESS.md) and [architecture](docs/ARCHITECTURE.md).

## Build and run

Requires macOS 14+ arm64, CMake 3.24+, and Apple clang/Swift from Xcode or Command Line Tools. Product binaries have no third-party runtime dependency. No Wine, Rosetta, QEMU or hypervisor is used by the product, and the engine makes no network calls.

```sh
scripts/build.sh
build/execon --self-check
open build/ExeCon.app
```

Outputs: `build/execon`, `build/libexecon_engine.a`, and `build/ExeCon.app`. The script ad-hoc signs the execution host. The bundle identifier is configured only in `config/product.xcconfig`.

On this host, Xcode setup/license acceptance is incomplete. The script successfully builds the app with the installed Command Line Tools Swift compiler. The Xcode project is provided but its build remains unverified. No agreement or system developer-directory setting is changed by the scripts. See [signing](docs/SIGNING.md).

Build the source-controlled test programs with isolated **development-only** tools:

```sh
scripts/bootstrap-dev-tools.sh --mingw
scripts/build-guests.sh
build/execon --stats build/guests/hello-x86_64.exe
build/execon --interp-only build/guests/hello-x86_64.exe
build/execon build/guests/hello-i686.exe
```

These hello programs have explicit entry points and no ordinary CRT startup. They print `Hello from ExeCon!`. Guest executables are generated locally and ignored by Git. The pinned llvm-mingw download is hash-checked; it is never linked into the engine. Set `EXECON_MINGW_ROOT` to use an existing llvm-mingw installation.

On other hosts, `scripts/build.sh` builds portable C++ components only. Native JIT and Swift app tests require Darwin arm64. `EXECON_SKIP_APP=1 scripts/build.sh` builds only C++ on macOS.

## CLI

```sh
build/execon --inspect --report-imports program.exe
build/execon --trace=all --dump-ir --dump-asm --stats program.exe arg1 'argument two'
build/execon --interp-only --on-missing=abort program.exe
mkdir -p build/guest-data
build/execon --drive "T=$PWD/build/guest-data" build/guests/files_heap-x86_64.exe
```

`--help` lists all current options. Options precede the executable; subsequent arguments belong to the guest. `--cwd` is a Windows directory, such as `'T:\'`. The default prefix is `~/Library/Application Support/ExeCon/prefix/`; C: maps to its `drive_c` subdirectory, Z: maps to host `/`, and `--drive` adds mappings. Create the prefix's `drive_c` directory before using it for file creation. Mapped host files are accessible to the guest; this is not a security sandbox.

`--env KEY=VALUE` constructs the guest environment explicitly. Major equivalents are `EXECON_PREFIX`, `EXECON_CWD`, `EXECON_ENV` (newline-separated assignments), `EXECON_INTERP_ONLY=1`, `EXECON_TRACE`, `EXECON_ON_MISSING`, `EXECON_STATS=1`, `EXECON_DUMP_IR=1`, `EXECON_DUMP_ASM=1`, `EXECON_LOG_LEVEL`, `EXECON_LOG_FILE`, `EXECON_MAX_INSTRUCTIONS` and `EXECON_STATUS_FILE`.

Unknown instructions and APIs fail with diagnostics. Default missing-API policy is `abort`. `return-error` only returns documented failure values for a small known signature catalog; it does not guess unknown return conventions. `ignore-void` currently permits the explicitly requested Sleep stub only. Detailed `--trace=mem` is not implemented and is rejected. `--dump-asm` prints emitted ARM64 instruction words, not symbolic assembly.

Guest exit codes are returned modulo 256, as required by POSIX process status. Engine codes are 125 (usage), 126 (engine/unsupported feature) and 127 (guest fault). Guest exits can collide with those values; `--status-file result.json` distinguishes the result kind and preserves the full 32-bit code. The C API in [execon_engine.h](engine/api/execon_engine.h) also separates engine errors from guest exit codes.

## Tests

```sh
scripts/run-tests.sh                 # Full currently-supported subset
scripts/run-tests.sh --unit          # Fast C++ unit checks only
EXECON_BUILD_DIR=build-asan scripts/build.sh -DEXECON_SANITIZE=ON
EXECON_BUILD_DIR=build-asan scripts/run-tests.sh
scripts/benchmark.sh
```

The full supported-subset command checks formatting, PE mutations, guest CLI/C API behavior, independent Unicorn state comparisons, Capstone decoder lengths and Swift streaming console behavior. Native tests are explicitly skipped in sanitizer/portable builds. The oracle is development-only; in a restricted automation sandbox it may require execution outside that sandbox. No external oracle is linked into the product.

Broader CRT/thread/TLS/SEH/C++/floating/registry fixtures and their expected outputs are present as **unmet gates**:

```sh
scripts/build-runtime-guests.sh
python3 tests/integration/runtime.py build
scripts/fetch-test-binaries.sh
scripts/run-acceptance.sh
```

These gates currently fail. The i686 SEH fixture also hits a compiler assembler-label error in the pinned llvm-mingw toolchain; its source is retained and the build failure is reported. `scripts/run-acceptance.sh --diagnose-only` regenerates import/startup reports without claiming acceptance. All downloads stay in ignored `third_party_bin/`. The coverage-guided PE fuzzer target exists (`-DEXECON_FUZZ=ON`), but this host's Apple compiler lacks the libFuzzer runtime; deterministic mutation tests run under ASan/UBSan.

## SwiftUI frontend

Use File > Open, a window drop, or the `.exe` document association. The console accepts stdin and basic ANSI colors; settings persist per executable. Run/Stop/Restart, history and Log & Trace are provided. Stop currently terminates the child host; Windows console control-event dispatch is not implemented. The app's startup check tests its bundled host's actual JIT execution. Interactive Finder/Dock/drop behavior still needs manual validation.

Windows GUI rendering remains a separate design phase; see [GUI_PHASE.md](docs/GUI_PHASE.md). Start future engine work with SSE2/decoder breadth, virtual memory and full CRT/TLS/DLL initialization, then exception dispatch and threading. The measured acceptance imports provide concrete priorities. Contributor instructions are in [AGENTS.md](AGENTS.md).
