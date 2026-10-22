# ADR 3: Checked guest faults before native signal recovery
Status: accepted for the initial subset

## Context
Precise guest exceptions are essential. Async POSIX signals cannot safely unwind arbitrary C++ containers or dispatch Windows SEH. The initial JIT performs no unchecked guest-memory access.

## Options
Install global process signal handlers now; use Mach exceptions; explicitly check guest access and retain PC metadata until direct memory operations exist.

## Decision
Raise access/illegal-instruction/breakpoint fault objects at interpreter boundaries. Store a precise guest PC for each native instruction and retain host-offset mappings. Do not install a misleading signal handler that turns engine defects into guest exceptions. A future direct-memory backend must select Mach exceptions or carefully constrained signal recovery with a new ADR before shipping.

## Consequences
No host SIGSEGV/SIGBUS/SIGILL recovery or Windows SEH dispatch is claimed. Faults currently terminate the guest diagnostically. Process isolation protects the SwiftUI app. This is an explicit incomplete portion of M3, not a substitute for the eventual SEH requirements.
