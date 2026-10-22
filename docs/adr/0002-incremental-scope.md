# ADR 2: Preserve a working subset across milestones
Status: accepted

## Context
The brief describes an entire clean-room Windows compatibility runtime. Full instruction coverage, MSVC C++ exception dispatch and third-party applications require substantially more implementation and validation than a single development run. No subset can honestly be labeled full compatibility.

## Options
Claim broad support through no-op stubs; stop after scaffolding; deliver a tested vertical slice with explicit unsupported boundaries.

## Decision
Proceed in milestone order through independent pieces, commit each validated increment, and label incomplete milestones partial. Initially support single-threaded, integer-only, explicit Win32 console programs. Preserve a checked interpreter as the reference engine. Reject unsupported startup metadata and execution semantics; record exact diagnostics. PE32 parsing and decoder design precede PE32 process execution.

## Consequences
The eventual acceptance gates (SSE2, SEH, CRT startup, multithreading, PE32/x87, 7-Zip and curl) remain unmet until their actual tests pass. Building the frontend does not imply compatibility with arbitrary executables. Later work can extend tested interfaces without pretending that a stub fulfills an API contract.
