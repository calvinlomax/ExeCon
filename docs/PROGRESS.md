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
