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
