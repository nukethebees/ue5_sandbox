Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++
* Prefer simple, explicit systems over speculative abstraction
* Heavy SoA container usage
* UI must not own gameplay logic
* Prefer implementing work under `native/` until Unreal Engine integration is needed. Local code avoids shared Unreal engine resource contention.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* Use `agent-task git` for mutating Git operations. It intentionally supports only the commands
  and options documented by `agent-task git --help` and each subcommand's `--help`. Within that
  subset, agents may freely commit, amend, rebase, reset, clean, and restore their own feature work.
  Report unsupported operations rather than bypassing the tool. Add capabilities only when a real
  workflow needs them; raw mutating Git requires an explicit maintainer-authorized exception.
* Git stash is intentionally unsupported because it is repository-global. To park work, create a
  temporary local feature branch and commit the WIP there; creating a branch alone does not save it.
  Do not use Git stash.
* `dev`, `main`, and `master` are protected regardless of ASCII case. Read them freely, but do not
  mutate them or switch an agent worktree onto them through the ordinary Git path.
* The worktree containing the current CWD is the workspace boundary. Do not access or modify
  another worktree outside that boundary. Git owns index/ref locking and operation state.
  Never mutate or attach to a branch checked out by another worktree; it may be a revision input.
  AgentTask-created worktrees live only under this workspace's ignored `.local/worktrees/` and
  are created/removed by branch name through `agent-task git worktree`.
* AgentTask is a cooperative guardrail, not a hostile-process security sandbox. Do not bypass it
  with raw mutating Git, direct `.git` edits, environment overrides, aliases, alternate Git
  executables, shell tricks, or other workarounds. Explicitly permitted read-only raw Git is fine.
  If blocked, report the reason; operations requiring an exception need maintainer intervention.
* `dev` is the integration branch
* Begin a new task with `agent-task prepare-worktree` from anywhere in the current Git worktree. It clears
  that worktree's `out`, initializes/updates submodules, and regenerates presets and code.
  It does not perform a broad project/test build; build only the targets needed for the task afterward.
  The canonical jobserver must be installed before preparation can clear `out`. The maintainer
  installs/updates `agent-task` with `. .\dev.ps1` then `install-agent-task` and manages PATH;
  agents invoke it by name from PATH. If it cannot be found or launched, halt and report the
  problem so the maintainer can fix it; do not use an absolute-path fallback or install it automatically.
  Install central per-user prerequisites separately with
  `agent-task install-central-tools`. See `tools/rust/README.md` for installation.
* After worktree preparation, build only affected targets and execute relevant CTest labels.
  Use `ctest --test-dir out/build/native -L <subsystem> -LE "soak|compile-contract"` for the
  fast loop. Include the applicable expensive categories once for final validation.
  `native-simulation-tests` is the ordinary-only workflow; `native-simulation-full-tests`
  adds the soak for final simulation validation. `native-tests` includes native soak/compile
  contracts but excludes standalone developer-tool tests.
* Run code/asset generators before starting the task and re-run as needed
* Perform feature work on dedicated feature branches
* Do not bypass instructions here unless explicitly told to
* Use the CMake workflows and jobserver described in [Builds](#builds). 
* Do not interfere with agents in other worktrees
* **Fast default:** 
  * Understand the task
  * Finish the coherent implementation and required cleanup
  * Format and review it
  * Run the smallest useful native/focused validation
  * Fix with focused checks then report ready
  * Only build and run what is needed.
* After the user authorizes integration, run `integrate-feature` from the feature
  worktree. This queues fairly for the exclusive `integration/dev` jobserver resource; ordinary
  feature work and unrelated jobserver resources remain concurrent.
  * Use `get-jobserver-state` or the `jobserver-status` target to inspect running and queued jobs.
  * When directly submitting a coordinated job, provide a descriptive operation name and an established jobserver kind; it should be identifiable from `jobserver status`, `show`, or `history`. 
  * Use queue metadata to understand ownership and contention; never cancel or kill another agent's job simply because it blocks yours.
  * Processes explicitly reported as owned by the current worktree by `jobserver process-owner` or `jobserver processes --owned` may be terminated with `jobserver kill-owned` without asking the user.
* Tooling and native-only candidates must not acquire Unreal resources unless their dependency
  surface requires Unreal. Run light, relevant tests after implementation; run expensive relevant
  gates once against the pinned final candidate.
* Integration performs a cheap final Git transaction. Complete affected builds, focused tests,
  static analysis and review before requesting integration authorization.
* A final-rebase conflict is aborted and releases the integration job. Resolve with
  `agent-task git rebase dev` outside the queue, validate the resolution, then requeue.
  Report the stopped stage, blocker, required action, and retained state.
* `agent-task integrate` is privileged and is not part of the unconditional Git permission surface.
  Invoke it through `integrate-feature` only after explicit user authorization.
* If AgentTask is broken, report it once and follow an explicit maintainer instruction for any
  minimal alternative; do not repeatedly retry or deliberately bypass it.
* You have permission to kill stale/hung processes that you spawned or were spawned in your worktree
* Do not chain CLI commands that may trigger an approval request when they wouldn't individually. This includes routing command outputs to log files. Read the CLI output directly yourself.
* Make commits for each discrete chunk of work as you work. Use good judgement.
* Try to avoid making just one commit for all the work


# Builds

* CMake is used to drive all builds, including UBT
* Load dev.ps1 when starting a task
* `ctools` refreshes standalone developer C# executables under `tools/bin`. CMake
  workflows build their configuration-local C# host-tool dependencies on demand; do not run
  `ctools` as a workflow preflight. Native mimalloc validation likewise builds its configuration-
  local `NativeBinaryTools` host dependency on demand.
* For final integration, only build and test what your work has affected
* Native C++ changes must pass the affected `clang-tidy-<scope>` workflows with no diagnostics
  before readiness or integration. Include consumers of changed shared headers; use the full
  `win-x64-clangcl-debug-tidy` workflow for core/memory/profiling public headers, shared compiler
  configuration, or uncertain/cross-cutting scope. See `docs/build-and-test.md` for scope rules.
  Inspect the selected `clang-tidy-<scope>.log` files (or `clang-tidy.log` for the full sweep);
  the runner itself does not make findings fatal.
* Keep benchmarks short; Not more than 3 minutes total
* Standalone developer-tool tests are not part of the default validation path. Run
  `cmake --workflow --preset tool-tests` only when the change can affect a tool or its tests, a
  directly consumed interface/protocol/file format/configuration, shared build or tool
  infrastructure, or the tool is being diagnosed. Unrelated native, game, and runtime changes must
  not run them merely because a broad test command exists. Select focused tool tests from the affected dependencies; use the complete developer-tool
  suite for shared/unknown tool infrastructure. Integration does not rerun validation.


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

Do not say “almost done”, “virtually done”, “nearly finished”, or give percentage-style completion estimates unless all required validation steps are already known and enumerated. When reporting progress, explicitly separate:
- implementation complete/incomplete
- tests complete/incomplete
- static analysis complete/incomplete
- sanitizer validation complete/incomplete
- integration/build validation complete/incomplete
- unresolved issues
If a new issue invalidates previous validation or requires additional builds/tests, explicitly retract the previous completion estimate.

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

* Do not run AddressSanitizer (ASAN) builds or tests unless the user explicitly requests them.
  ASAN is not a default validation, readiness, or integration requirement.
* Only create tests when explicitly asked.
* Use CQTest/Unreal automation tests for code with engine/editor dependencies.
* Register Unreal Automation benchmarks under the top-level `SandboxBenchmarks` category.
* Prefer putting tests in native where possible
* Prefer running native tests as part of the normal flow
* Leave Unreal tests until the end of a task
