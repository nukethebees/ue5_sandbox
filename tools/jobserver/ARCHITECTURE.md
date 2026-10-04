# Jobs-board architecture

The board stores cooperative scheduling tickets. Callers run commands themselves and report
when work starts and ends. It never executes, monitors, or terminates processes.

## Responsibilities

| Component | Responsibility |
| --- | --- |
| [coj jobs](../rust/crates/coj/src/jobs.rs) | Parse commands, attach owner/worktree metadata, and format output. |
| [coj install](../rust/crates/coj/src/install.rs) | Build jobserverd through CMake; install, register, and start it from Rust. |
| [jobserver-client crate](../rust/crates/jobserver-client/src/lib.rs) | Exchange framed JSON requests with the per-user daemon and interpret errors. Linked into coj; no executable. |
| [board.hpp](server/lib/include/jobserver/server/board.hpp) / [board.cpp](server/lib/src/board.cpp) | Store tickets, schedule admission, and apply transitions or explicit clearing. |
| [server.cpp](server/lib/src/server.cpp) | Validate requests and dispatch board operations and daemon administration. |
| [protocol.hpp](lib/include/jobserver/protocol.hpp) / [protocol.cpp](lib/src/protocol.cpp) | Define server-side framing, payload limits, and protocol version. |
| [platform/](platform/) | Serve same-user local clients through Windows named pipes. |
| [daemon_main.cpp](platform/windows/daemon_main.cpp) | Start jobserverd. |

The Rust client and C++ daemon use protocol version 5: UTF-8 JSON preceded by a four-byte
little-endian payload length, limited to one MiB. Each request carries the protocol version.
A mismatch fails explicitly; client and daemon must be updated together.

## Ownership

`coj codex start <name>` sets `COJ_CODEX_NAME` and `COJ_CODEX_WORKTREE` for descendants.
The jobs command uses that name only when the current physical worktree matches the inherited
worktree. Outside a named session it uses the worktree directory name, or the current directory
when outside Git. `request --owner` overrides the name.

Worktree paths are canonicalized and normalized for Windows case comparisons. The server
stores and compares these strings without inspecting files, sessions, or processes. Ownership
is a display/filtering aid, not an authorization boundary. Session names may be reused.

## Scheduling and lifetime

Shared tickets may run together. Exclusive tickets wait for earlier tickets to finish and
block later shared work. Ready tickets reserve their place until started or cancelled.

```text
Queued -> Ready -> Running -> Done
Queued/Ready -> Cancelled
Any active state -> removed by explicit clear
```

Tickets exist only in memory and survive client disconnection. There is no automatic expiry,
completion, owner-liveness check, or crash recovery. Ending, cancelling, and clearing remove
tickets and recalculate readiness. Owner clearing is one server operation, optionally filtered
by worktree. It cannot stop the owner's work. Shutdown requires an empty board.

## Build and validation

Build `jobserverd`, `jobserver-board-tests`, `jobserver-tests`, and `rust-tool-tests-build`
through the native CMake preset. Then run:

```powershell
ctest --test-dir out/build/native -L "jobserver|coj" --output-on-failure
```

Portable C++ tests cover the board and framing. Existing CLI syntax coverage lives in
coj' Rust tests. The client crate's migrated integration tests communicate with
a CMake-built `jobserver-test-server` on unique named pipes. CTest supplies the fixture path and
runs those explicitly ignored Cargo tests; they never contact the installed board.
