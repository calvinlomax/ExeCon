# Compatibility

Status on 2026-10-23: **experimental integer console subset, not a general Windows executable runner.** GUI rendering and HTTPS through Schannel/SSPI are out of scope. SSE/SSE2, x87, SEH/C++ exceptions, threading and Winsock are not implemented. CPUID does not advertise optional features. Unsupported instructions raise a diagnostic guest illegal-instruction fault; missing imports default to abort.

| Program / test | PE32+ native dispatcher | PE32+ interpreter | PE32 interpreter | Notes |
|---|---|---|---|---|
| `hello.c` | Pass | Pass | Pass | llvm-mingw, explicit entry, no CRT startup |
| `console.c` | Pass | Pass | Pass | UTF-16 λ and surrogate-pair rocket output |
| `echo.c` | Pass | Pass | Pass | Binary stdin/stdout including NUL and invalid UTF-8 |
| `exit.c` | Pass | Pass | Pass | Guest exit 42 and full status JSON |
| `startup.c` | Pass | Pass | Pass | GS/FS TEB self-pointer, PEB image base |
| `crt_subset.c` | Pass | Pass | Pass | Imported malloc/puts/free; cdecl on PE32 |
| `files_heap.c` | Pass | Pass | Pass | File create/read/write/seek/close, process heap realloc |
| `environment.c` | Pass | Pass | Pass | Explicit environment lookup and QPC frequency |
| `missing.c` | Diagnostic pass | Diagnostic pass | Diagnostic pass | Default failure; explicit MessageBox failure return policy |
| 7-Zip `7za.exe` 26.05 x64 | **Fails** | **Fails** | Not this artifact | First call to missing `msvcrt!__set_app_type` |
| 7-Zip `7z.exe` 26.05 x64 | **Fails** | **Fails** | Not this artifact | Same CRT startup blocker; real `7z.dll` loading also absent |
| curl 8.23.0_4 x64 | **Fails** | **Fails** | Not this artifact | Rejected at load-config initialization |

“Native dispatcher” means supported register-only blocks execute actual ARM64 code and other supported blocks execute IR. It does not mean every instruction is translated. PE32 currently always uses the interpreter. A passing test says nothing about broader applications that share a DLL name.

## Acceptance evidence

Official packages and hashes are pinned in [binaries.json](../tests/acceptance/binaries.json); downloaded executables are ignored and never packaged with ExeCon. Full per-import reports and run diagnostics are in [acceptance/](acceptance/).

- **7za:** 121 missing imports, 24 implemented. [Exact import report](acceptance/7za.exe.imports.txt). Beyond `__set_app_type`, this includes `__getmainargs`, `_initterm`, `_iob`, `_CxxThrowException`, `__CxxFrameHandler`, `_beginthreadex`, `VirtualAlloc`, `WaitForSingleObject`, directory enumeration, registry/security and OLE automation entry points.
- **7z:** 113 missing imports, 23 implemented. [Exact import report](acceptance/7z.exe.imports.txt). It additionally needs real PE DLL initialization, exports/forwarders and dynamic module APIs to use `7z.dll`.
- **curl:** 252 missing imports, 24 implemented. [Exact import report](acceptance/curl.exe.imports.txt). Requires load-config/TLS/CRT startup, substantial UCRT stdio/locale/string/environment support, thread/synchronization/VM APIs and the imported Winsock surface (`WSAStartup`, sockets, DNS, select, event APIs, send/recv, etc.). It also imports LDAP, certificate, IDN and security interfaces; only exercised APIs should be implemented, and unused imports remain diagnostic stubs. `Secur32!InitSecurityInterfaceA` is not emulated. **HTTPS through Schannel/SSPI is explicitly unsupported.** The acceptance HTTP fixture is localhost-only and uses plain `http://`.

Archive listing/creation/extraction and curl HTTP are **not passing acceptance tests**. `scripts/run-acceptance.sh` exits nonzero while these targets fail; `--diagnose-only` generates reports and expressly does not claim a pass. Skipping a startup requirement or returning success from a stub would invalidate these gates.

## Boundaries

- CPU: integer subset documented by `execon --decoder-coverage`. AVX is out of scope. x87/SSE, atomic LOCK, REP strings, multiply/divide, ADC/SBB and most extended instructions are absent.
- Loader: PE parsing is broader than runtime support. TLS/load-config/delay-load/managed startup are rejected. Resource trees and bound-import semantics are not implemented. File/image limits are 256 MiB.
- Exceptions: checked access/illegal-instruction/breakpoint faults are diagnosed and terminate the guest. Neither x64 unwind dispatch nor x86 SEH registration dispatch is implemented. MSVC C++ exceptions do not work.
- Files: synchronous regular files and duplicated standard streams. ASCII case lookup and process-local share checks only. UNC, alternate streams, overlapped I/O, file mappings, directory enumeration and full security semantics are absent. A APIs use UTF-8 (ACP 65001).
- Memory: checked sparse guest pages and page-backed process heap. VirtualAlloc/Protect/Query wrappers, custom heaps and heap reuse are absent. Guest physical allocation cap is 256 MiB.
- Process: one guest thread. No registry, networking, LoadLibrary/GetProcAddress, console control-event dispatch or real DLL initializers. SIGINT/Stop currently terminates the CLI host.
- Frontend: compiled and signed on this arm64 host; interactive Finder/Dock/drop behavior still needs manual validation. Basic SGR colors, not a full terminal. CLI preserves binary streams; UI renders text with replacement characters for invalid UTF-8 and bounds display memory.
- Platform: minimum target macOS 14, tested on this host only; no separate Sonoma hardware validation. Xcode project build awaits license/setup; Command Line Tools fallback built the app.
