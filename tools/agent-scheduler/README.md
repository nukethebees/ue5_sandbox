# Modified Codex scheduler

Launch the canonical installation with `agent-codex.ps1`. It connects once to the canonical
[jobserver](../jobserver/README.md) and loads separate scheduling exemption rules.

Cheap exempt commands run normally. For an expensive command, make two separate tool calls:

```text
agent-scheduler ticket shared "compile"
cmake --build --preset native --target my-target
```

For a benchmark, request `exclusive` instead of `shared`, then run the ordinary benchmark command.
`agent-scheduler status` inspects your ticket; `agent-scheduler clear` discards an unused ticket.
These are internal Codex pseudo-commands, not executables or helper connections.

Codex waits for a queued ticket, executes the logical command, and releases when that call returns.
Only one ticket and one scheduling operation may be outstanding. Exemptions never grant security approval.
Missing components or connection loss fail closed; ask the maintainer to fix the installation.

See [behavior and limits](docs/behavior.md) and [installation/development](docs/development.md).
