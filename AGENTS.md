Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* Data is primarily stored in SoA containers
* UI must not own gameplay logic
* Determinism, debuggability, simple control flow, and performance are important.
* Prefer implementing work under `native/` until Unreal Engine integration is needed, then add a
  thin adapter after the native implementation is substantially settled. Local native validation is
  the fast path and avoids contention on shared Unreal engine resources.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* Use the canonical absolute `agent-git` executable for supported Git mutations. Raw Git is
  acceptable for read-only inspection; or if agent-git is non-functional
* `dev` is the integration branch. Perform feature work on dedicated feature branches 
* Use the CMake workflows and repository jobserver described in [Builds](#builds) for expensive
  jobs. Do not bypass that coordination or interfere with jobs owned by other worktrees or agents.
* **Fast default:** understand the task; finish the coherent implementation and required cleanup;
  format and review it; run the smallest useful native/focused validation; fix with focused checks;
  then report ready. Do not routinely stop to build or test intermediate states. Do so only when a
  result is needed to continue—for example, to resolve uncertain compiler/API or generated-code
  behaviour, check a consequential assumption cheaply, or debug an observed failure.
* Use the smallest non-overlapping validation that answers the question; do not run broader suites
  merely for reassurance. For trivial non-functional changes such as documentation or comments, do
  not run tests by default; state that tests were not run because the change was trivial.
* Prefer multiple coherent commits for substantive work
* A feature is **ready for integration** when its implementation is formatted, reviewed, and given
  credible light/focused validation, and its commits are coherent. Do not repeatedly rebase merely
  because `dev` advanced; rebase during development only for a specific known dependency. Report
  readiness and wait for the user's explicit authorization without acquiring a reservation.
* After the user explicitly authorizes integration, run `integrate-feature` from the feature
  worktree. This queues fairly for the exclusive `integration/dev` jobserver resource; ordinary
  feature work and unrelated jobserver resources remain concurrent. 
* If rebase issues can be fixed quickly, fix them, otherwise release the lease
* If tooling like agent-git is broken, note it and then fall back on alternatives

# Builds

* CMake is used to drive all builds, including UBT
* Load dev.ps1 when starting a task
* ctools will build essential tools
* A canonical per-user jobserver coordinates expensive work across worktrees. Ordinary work shares the machine resource; benchmarks wait for older work to drain and then run exclusively. Use `get-jobserver-state` or the `jobserver-status` target to inspect running and queued jobs. Continue to use repository CMake/PowerShell wrappers for coordinated work; do not bypass them merely to customize queue metadata.
* Prefer to build native code for the development process; leave Unreal builds to the end of a task to avoid UBT mutex contention
* Run code/asset generators needed for the task
* Keep benchmarks short; More than 3 minutes total is too long


# Agent Behaviour

* Do not guess. Ask questions when things are unclear.
* Use targeted repository inspection to establish implementation facts.
* Do not inspect Unreal Engine source unless needed to resolve an engine/API behaviour question.
* Once enough context exists, implement rather than continuing exploration.
* Prefer the smallest coherent change that fully implements the requested design.
* Keep README.md files concise and user-oriented. Put detailed notes in an adjacent ARCHITECTURE.md.
* When directly submitting a coordinated job, provide a descriptive operation name and an established jobserver kind; another human or agent must be able to identify its work from `jobserver status`, `show`, or `history`. Use `NUKETHEBEES_JOBSERVER_TASK` for a session label only when branch-derived task attribution is insufficient, and never invent worktree provenance. Use queue metadata to understand ownership and contention; never cancel or kill another agent's job simply because it blocks yours.
* Treat obvious temporary contention for shared resources, locks, job slots, or capacity as a wait condition: do not repeatedly retry across consecutive turns; sleep within the shell/tool invocation before retrying with 10s, then 30s, then 60s backoff (capped at 60s). Investigate or report the failure if it changes, appears non-transient, or persists unreasonably.
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
