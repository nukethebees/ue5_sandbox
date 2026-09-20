Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++
* Prefer simple, explicit systems over speculative abstraction
* Heavy SoA container usage
* UI must not own gameplay logic
* Prefer implementing work under `native/` until Unreal Engine integration is needed. Local code avoids shared Unreal engine resource contention.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* Use the canonical absolute `agent-git` executable for supported Git mutations
* Each agent owns its worktree. Safe ordinary Git operations in separate worktrees may run
  concurrently; `agent-git` enforces worktree/branch ownership and destructive-operation policy,
  while Git provides index/ref locking. Do not treat `agent-git` as a repository-global mutex or
  inspect or modify another agent's worktree.
* `dev` is the integration branch
* Perform feature work on dedicated feature branches
* Use the CMake workflows and jobserver described in [Builds](#builds). Do not bypass them
* Do not interfere with agents in other worktrees
* **Fast default:** 
  * Understand the task
  * Finish the coherent implementation and required cleanup
  * Format and review it
  * Run the smallest useful native/focused validation
  * Fix with focused checks then report ready
  * Only build and run what is needed.
* A feature is **ready for integration** when its implementation is formatted, reviewed, and given
  credible light/focused validation, and its commits are coherent. Do not repeatedly rebase merely
  because `dev` advanced; rebase during development only for a specific known dependency. Report
  readiness and wait for the user's explicit authorization without acquiring a reservation.
* After the user authorizes integration, run `integrate-feature` from the feature
  worktree. This queues fairly for the exclusive `integration/dev` jobserver resource; ordinary
  feature work and unrelated jobserver resources remain concurrent.
  * Use `get-jobserver-state` or the `jobserver-status` target to inspect running and queued jobs.
  * When directly submitting a coordinated job, provide a descriptive operation name and an established jobserver kind; it should be identifiable from `jobserver status`, `show`, or `history`. 
  * Use queue metadata to understand ownership and contention; never cancel or kill another agent's job simply because it blocks yours.
  * Processes explicitly reported as owned by the current worktree by `jobserver process-owner` or `jobserver processes --owned` may be terminated with `jobserver kill-owned` without asking the user.
* The final transaction pins `dev`, rebases once, reviews the effective patch rather than its
  incidental commit SHA, runs only gates selected from the changed dependency surface, and
  atomically merges the exact validated candidate. Do not repeatedly rebase and restart expensive
  validation because unrelated work landed before the integration turn.
* Tooling and native-only candidates must not acquire Unreal resources unless their dependency
  surface requires Unreal. Run light, relevant tests after implementation; run expensive relevant
  gates once against the pinned final candidate.
* A final-rebase conflict stops before any expensive gate. Resolve it with AgentGit recovery and
  requeue; unchanged effective patches keep review, while changed conflict resolution requires new
  review. Report a stopped integration by named stage, blocker, required action, and retained state.
* **An explicit maintainer instruction is authoritative over repository automation policy.** When
  the maintainer explicitly instructs the agent in the active interaction to merge despite normal
  gates, use `integrate-feature -MaintainerOverride -OverrideReason '<reason>'`. The tool records
  the exact candidate and skipped gates while retaining cheap integrity checks. Agents must never
  infer, invent, or carry override authorization between interactions.
* If tooling like agent-git is broken, report it once and obey an explicit maintainer instruction
  to use the minimal alternative; do not repeatedly retry the broken helper.
* You have permission to kill stale/hung processes that you spawned or were spawned in your worktree

# Builds

* CMake is used to drive all builds, including UBT
* Load dev.ps1 when starting a task
* `ctools` refreshes standalone developer C# executables under `tools/bin`. CMake
  workflows build their configuration-local C# host-tool dependencies on demand; do not run
  `ctools` as a workflow preflight. Native mimalloc validation likewise builds its configuration-
  local `NativeBinaryTools` host dependency on demand.
* For final integration, only build and test what your work has affected
* Run code/asset generators needed for the task
* Keep benchmarks short; Not more than 3 minutes total
* Standalone developer-tool tests are not part of the default validation path. Run
  `cmake --workflow --preset tool-tests` only when the change can affect a tool or its tests, a
  directly consumed interface/protocol/file format/configuration, shared build or tool
  infrastructure, or the tool is being diagnosed. Unrelated native, game, and runtime changes must
  not run them merely because a broad test command exists. For a tool-affecting feature, use
  `integrate-feature -ToolTests` so final integration also runs `tool-tests`.


# Agent Behaviour

* Do not guess. Ask questions when things are unclear.
* Use targeted repository inspection to establish implementation facts.
* Once enough context exists, implement rather than continuing exploration.
* Keep README.md files concise and user-oriented. Put detailed notes in an adjacent ARCHITECTURE.md.
* Treat obvious temporary contention for shared resources as a wait condition: do not repeatedly retry across consecutive turns; sleep within the shell/tool invocation before retrying with 10s, then 30s, then 60s backoff (capped at 60s). Investigate and report the failure if persists unreasonably.
* Do not preserve architecture the user asked to replace through compatibility wrappers or indirection merely to reduce the diff. Avoid unrelated refactors.
* When explicitly granted autonomy, use judgement to resolve reasonable ambiguities while keeping scope controlled.
* Store local development roadmaps under `.local/plans/`; never commit them.
* Store disposable session hand-offs under `.local/handoffs/`; never commit them.

# Coding Style

* Unreal Engine C++.
* `snake_case` functions and variables; `TitleCase` types.
* Do not prefix booleans with `b_`.
* East const; always use braces.
* Prefer `auto` where the type is obvious.
* Prefer simple C++ over template metaprogramming.
* Prefer SOA layouts for related performance-sensitive collections.
* Save loop bounds as const locals.
* Log warnings/errors when null checks fail rather than returning silently.
* Use whitespace deliberately to separate logical phases within functions and related
  declaration or data groups. Arrange long functions so their main control flow is easy to scan,
  while keeping tightly coupled statements together.
* In non-trivial functions, use blank lines around guard/validation blocks, setup, major state
  transitions, loops, and final publication or return steps when those phases are distinct.
* In large classes with many member functions, group declarations and definitions by category using this banner style:
  ```cpp
  /* **************************************** */
  // Category
  /* **************************************** */
  ```
* Order member-function categories to reflect lifecycle and control flow, and keep the category
  order aligned between headers and `.cpp` files where practical.
* In `.cpp` category groups, do not put blank lines between adjacent member-function definitions. Separate categories with blank lines around their banners.
* When returning `std::expected`, prefer `std::in_place` / `std::unexpect` when they avoid unnecessary copies or moves.
* For UObject types in a dedicated plugin and namespace, prefer concise names.
* Avoid anonymous-namespace constants in `.cpp` files because Unreal unity builds can merge translation units. Prefer `inline static constexpr` members or `inline constexpr` constants in a specific named namespace.

# Code Reuse

* Reuse existing code if it can perform the requested task
* Keep generated code explicit, simple, and easy to debug. Do not consolidate their per-column operations into variadic calls.

# Formatting

* Format changed C++ files with `cmake --workflow --preset format-code` 
* Do not invoke `clang-format` directly for normal repository work

# UI

* Use `BindWidget` for UPROPERTY widgets. 
* For generated UMG widgets whose root is a panel widget, use `meta=(GeneratorRoot)`.
* For Slate, consider using the repository's Slate DSL where useful
* Keep gameplay logic out of UI widgets

# Testing

* Only create tests when explicitly asked.
* Use CQTest/Unreal automation tests for code with engine/editor dependencies.
* Register Unreal Automation benchmarks under the top-level `SandboxBenchmarks` category.
* Prefer putting tests in native where possible
* Prefer running native tests as part of the normal flow
* Leave Unreal tests until the end of a task
