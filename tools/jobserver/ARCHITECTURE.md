# Jobserver architecture

The per-user `jobserverd.exe` daemon owns queue state, resource accounting, process supervision,
history, and logs. `jobserver.exe` and the `jobserver::client` C++ library communicate through
user-SID-scoped local named pipes; there is no remote scheduler.

Jobs acquire all claims atomically. Counted, shared, and exclusive claims support CPU budgets,
ordinary machine work, benchmarks, GPU work, and engine-specific Unreal read/write gates. Older
exclusive requests block newer shared users so they cannot starve. Nested commands retain their
outer supervised job rather than queueing a deadlocking child claim.

Supervised commands live in kill-on-close Windows Job Objects. Attached jobs end with their client;
detached jobs retain capped output and history. A connection-owned lease instead contains a local
critical-section process tree, releasing its claim and terminating descendants if its owner exits.

The versioned UTF-8 JSON protocol validates requests before admission, bounds handlers and payloads,
and reserves a control endpoint for status and recovery while normal admission is busy. Logs and
history are diagnostic only: a restarted daemon never reconstructs live ownership from disk.

See [README.md](README.md) for installation and common commands.
