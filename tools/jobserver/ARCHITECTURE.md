# Jobs-board architecture

The jobserver is a cooperative, per-user board for coordinating heavyweight work. It tracks
tickets; callers run their own commands and report when work starts and ends.

## Source responsibilities

Client and server code have separate directories, include paths, and namespaces. Each library
keeps public headers under its `lib/include/` directory and implementations under `lib/src/`.

| Directory | Namespace | Responsibility |
| --- | --- | --- |
| `client/lib/` | `jobserver::client` | Send requests and interpret replies. |
| `client/cli/` | `jobserver::client::cli` | Provide the command-line interface. |
| `server/lib/` | `jobserver::server` | Own the board and serve requests. |
| `lib/` | `jobserver` | Share protocol, identifiers, errors, and encoding utilities. |
| `platform/` | `jobserver::platform` | Provide OS-specific communication behind a common interface. |

| Header / source pair | Responsibility |
| --- | --- |
| [request.hpp](client/lib/include/jobserver/client/request.hpp) / [request.cpp](client/lib/src/request.cpp) | Send JSON requests through the platform transport and interpret server replies. |
| [protocol.hpp](lib/include/jobserver/protocol.hpp) / [protocol.cpp](lib/src/protocol.cpp) | Define message framing, protocol version, and payload limits. |
| [transport.hpp](platform/include/jobserver/platform/transport.hpp) | Define the common local transport interface for clients and servers. |
| [client_transport.cpp](platform/windows/client_transport.cpp) | Connect to the Windows named pipe and exchange a request and reply. |
| [server_transport.cpp](platform/windows/server_transport.cpp) | Listen for Windows named-pipe connections, restrict access, and dispatch requests. |
| [transport_common.hpp](platform/windows/transport_common.hpp) / [transport_common.cpp](platform/windows/transport_common.cpp) | Share Windows user identity, endpoint naming, and framed pipe I/O. |
| [path_encoding.hpp](lib/include/jobserver/path_encoding.hpp) / [path_encoding.cpp](lib/src/path_encoding.cpp) | Convert native paths and Windows command-line arguments to UTF-8. |
| [commands.hpp](client/cli/commands.hpp) / [commands.cpp](client/cli/commands.cpp) | Define the CLI11 command interface, validate arguments, provide help, and format replies. |
| [board.hpp](server/lib/include/jobserver/server/board.hpp) / [board.cpp](server/lib/src/board.cpp) | Own tickets and enforce scheduling, readiness, and state-transition rules. |
| [server.hpp](server/lib/include/jobserver/server/server.hpp) / [server.cpp](server/lib/src/server.cpp) | Dispatch requests to the board and handle daemon administration. |

The data-only headers have no corresponding source file:

| Header | Responsibility |
| --- | --- |
| [ticket_id.hpp](lib/include/jobserver/ticket_id.hpp) | Define the shared ticket identifier type. |
| [ticket.hpp](server/lib/include/jobserver/server/ticket.hpp) | Describe a server-owned ticket, its scheduling mode, and its state. |
| [error.hpp](lib/include/jobserver/error.hpp) | Define the shared error representation. |

[cli_main.cpp](platform/windows/cli_main.cpp) connects command-line parsing, the client, and
output. [daemon_main.cpp](platform/windows/daemon_main.cpp) starts the server.

## Request flow and platform boundary

`agent-task jobs` forwards commands to the installed `jobserver` CLI. Requests pass through
CLI parsing, the client, the platform transport, and server dispatch to the board. Replies
return through the same layers.

The platform layer owns local communication and access control. The common client/server
code owns message interpretation and scheduling decisions. Requests are served sequentially,
and each connection carries one request and response.

Only Windows transport and entry points are implemented. The board, framing, path encoding,
and CLI parsing are independent of the Windows backend. Supporting another OS requires a
transport implementation, entry points, installation support, and CMake selection. The new
backend must preserve local same-user access and sequential request handling.

## Scheduling and lifetime

Shared tickets may run together. Exclusive tickets wait for earlier tickets to finish and
block later shared work. Ready tickets reserve their place until the caller starts or
cancels them.

```text
Queued -> Ready -> Running -> Done
Queued/Ready -> Cancelled
```

Tickets remain active independently of client connections and exist only in memory. The
server neither runs nor monitors commands, and tickets do not expire or finish automatically.
Callers must report completion; forgotten tickets can block the board. Shutdown requires an
empty board, and restarting the daemon loses all tickets.

See the [client guide](docs/client.md) for the command workflow and the
[server guide](docs/server.md) for administration.

## Build and validation

[CMakeLists.txt](CMakeLists.txt) only adds component subdirectories. Each component owns its
targets and their build requirements:

| CMake file | Responsibility |
| --- | --- |
| [lib/CMakeLists.txt](lib/CMakeLists.txt) | Build the shared `jobserver-core` library. |
| [platform/CMakeLists.txt](platform/CMakeLists.txt) | Select and build the OS transport backend. |
| [client/CMakeLists.txt](client/CMakeLists.txt) | Add the client library and CLI subdirectories. |
| [client/lib/CMakeLists.txt](client/lib/CMakeLists.txt) | Build `jobserver-client`, which sends requests to the daemon. |
| [client/cli/CMakeLists.txt](client/cli/CMakeLists.txt) | Build the testable `jobserver-cli` parsing/output library and the `jobserver` client executable. |
| [server/CMakeLists.txt](server/CMakeLists.txt) | Add the server library, executable, and test subdirectories. |
| [server/lib/CMakeLists.txt](server/lib/CMakeLists.txt) | Build the board and request dispatcher libraries. |
| [server/cli/CMakeLists.txt](server/cli/CMakeLists.txt) | Build the `jobserverd` daemon executable. |
| [server/tests/CMakeLists.txt](server/tests/CMakeLists.txt) | Build and register board tests. |
| [tests/CMakeLists.txt](tests/CMakeLists.txt) | Build and register protocol, CLI, and client/server integration tests. |
| [install/CMakeLists.txt](install/CMakeLists.txt) | Define executable installation and the per-user registration target. |

Platforms without a backend build the portable libraries and tests. Windows additionally
builds the client, server, executables, installation targets, and named-pipe integration tests.

Build `jobserver`, `jobserverd`, `jobserver-board-tests`, and `jobserver-tests`, then run
`ctest --test-dir out/build/native -L jobserver --output-on-failure`.

Board, protocol, and CLI parsing tests cover the portable components.
[tests/windows/server_tests.cpp](tests/windows/server_tests.cpp) exercises requests through
an isolated named-pipe server. AgentTask's focused Cargo tests cover its CLI facade.
