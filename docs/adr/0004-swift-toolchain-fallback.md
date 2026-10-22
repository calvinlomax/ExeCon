# ADR 4: Build the app with swiftc when Xcode is uninitialized
Status: accepted

## Context
Full Xcode is installed but refuses work pending license acceptance. Command Line Tools provide a working macOS SDK and Swift compiler.

## Options
Accept an agreement for the user; leave the app unbuilt; use the installed compiler without accepting new terms.

## Decision
Keep the Xcode project as the normal build. If xcodebuild -checkFirstLaunchStatus fails, compile the same sources and bridging header with Command Line Tools swiftc, using the same deployment target and configurable bundle identity. Package the same Info.plist and signed CLI child. Sanitizer builds remain CLI-only.

## Consequences
A usable signed app artifact is produced. The Xcode project itself remains unverified until the user completes Xcode setup. No system developer-directory settings are modified. EXECON_SWIFTC_APP=1 forces the fallback; EXECON_SKIP_APP=1 builds only C++ components.
