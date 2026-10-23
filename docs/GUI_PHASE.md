# Future Windows GUI phase — design input, no decision yet

No USER32/GDI/comctl32 rendering is implemented. The SwiftUI app is a host console frontend, not a Windows GUI renderer. GUI imports bind the same diagnostic tokens as other missing functions. Default policy aborts; explicitly selected return-error can return failure for MessageBox, GetMessage and CreateWindowEx signatures. No GUI function silently reports success.

## Options to evaluate

| Approach | Potential advantages | Hard problems |
|---|---|---|
| Map USER32 to AppKit, GDI to Core Graphics | Native accessibility/window integration; host text and drawing services | Win32 synchronous SendMessage/reentrancy, subclassing, pixel metrics, owner-drawn controls, DPI, input methods, AppKit main-thread restrictions |
| Implement a guest window/message system and software surface, display through SwiftUI | Consistent Win32 coordinates/compositing; easier deterministic screenshot tests and guest-owned drawing state | Text shaping, input/accessibility, complex controls, painting invalidation, performance, clipboard/IME integration |
| Hybrid surfaces and selected native controls | Incremental utility-app coverage | Two ownership models, focus synchronization and inconsistent behavior; could be harder than either complete approach |

These are alternatives for a later design review. No final choice is made here.

## Message loops and threading

Windows permits thread-owned windows and synchronous cross-thread messaging. AppKit requires main-thread UI work. A renderer must never execute guest callbacks directly on an arbitrary AppKit callback stack. Define a guest scheduler and explicit callback requests with guest ABI frames, cancellation and exception boundaries. Model posted versus sent messages, nested modal loops, timers, destruction ordering and GetMessage's 0/-1 distinction. Avoid blocking the host main thread while a guest thread waits for a response it must itself dispatch.

The existing CLI/UI process split should remain. Consider a versioned local IPC protocol carrying surface updates, input events, clipboard requests and accessibility metadata. Pipes suffice for today's console; structured bidirectional framing or XPC should be evaluated for GUI traffic. Never put guest or native pointers in cross-process messages. The app owns host window identities; the engine owns Windows handle lifetimes and guest callbacks.

## Existing hooks and prerequisites

- Guest import tokens and Win32 adapters are separate from NT service objects and handles.
- Guest CPU state, ABI argument handling and explicit guest-PC IR boundaries can support callback entry/return later.
- Checked memory permissions and write generations can support shared drawing buffers and executable callback invalidation.
- Per-process execution and diagnostics keep guest crashes out of the SwiftUI process.
- Before rendering: complete guest threads, wait/message scheduling, exception dispatch, Unicode/string behavior, resources, real DLL initialization and a typed object manager beyond file handles.

## Proposed evaluation gates

Start with hand-built tests for a simple window/message loop, paint invalidation, synchronous versus posted messages, timers, focus, resize/DPI, menus, modal dialogs, subclassing and keyboard/IME input. Add deterministic surface/image comparisons plus manual accessibility checks. Test destruction during callbacks and nested modal loops. Select an approach only after these prototypes expose the threading and ownership costs. Productivity-app compatibility must be measured per program; a successful MessageBox is not general GUI support.
