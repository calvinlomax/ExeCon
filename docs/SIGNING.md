# macOS signing and JIT

ExeCon targets macOS 14+ arm64. The CLI host owns the executable code arena. It uses MAP_JIT, pthread_jit_write_protect_np and sys_icache_invalidate. Guest pages are separate checked software memory, not executable host mappings.

`scripts/build.sh` signs the CLI and native test executables using hardened runtime and `config/jit.entitlements`. The only JIT entitlement is `com.apple.security.cs.allow-jit`. `scripts/sign.sh <binary>` uses ad-hoc identity `-` unless `EXECON_SIGN_IDENTITY` is set.

```sh
scripts/sign.sh build/execon
build/execon --self-check
codesign --display --entitlements :- build/execon
codesign --verify --strict build/execon
```

The app executes `Contents/Helpers/execon`; its main executable is `Contents/MacOS/ExeCon`. These must be in different directories because common macOS filesystems treat those names as identical. Sign the child first, then the outer bundle. The SwiftUI executable does not need allow-jit because it only calls the C API version function and launches the child host. `scripts/build-app.sh` packages and verifies both signatures.

```sh
codesign --verify --deep --strict build/ExeCon.app
build/ExeCon.app/Contents/Helpers/execon --self-check
```

If MAP_JIT fails, check the executable being launched and its entitlements, rebuild/re-sign it, or use `--interp-only`. Sanitizer builds intentionally disable native JIT. The app's first-run check actually executes a small native block; it does not infer readiness merely from a plist.

## Developer ID and notarization (optional distribution work)

For distribution outside this checkout, use an appropriate Developer ID Application certificate via `EXECON_SIGN_IDENTITY`, sign all nested code before the app, archive the app, and submit the archive with Apple's `notarytool` using credentials stored securely in a keychain profile. Staple an accepted notarization ticket and assess the final artifact with Gatekeeper. This workflow is not automated or claimed complete here. Ad-hoc signing is sufficient for the local development tests; it is not notarization or an Apple distribution identity.

See Apple's [JIT entitlement documentation](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-jit) and [notarization overview](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution). No private identity, certificate, account password or API key is stored in the repository.

## Build environment limitation

Xcode is installed on the development host but its license/setup is incomplete. The project remains present and syntax-validated; its build has not been verified. The build script uses the installed Command Line Tools Swift compiler as a fallback without accepting any agreement for the user. After completing Xcode setup, the ordinary build will select the Xcode project automatically. `EXECON_SWIFTC_APP=1` forces the fallback; `EXECON_SKIP_APP=1` builds only C++.
