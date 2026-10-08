Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++
* Prefer simple, explicit systems over speculative abstraction
* Heavy SoA container usage
* UI must not own gameplay logic
* Prefer implementing work under `native/` until Unreal Engine integration is needed. Local code avoids shared Unreal engine resource contention.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* `coj` is the project CLI for automating and controlling the development workflow.
  * At the start of a task, run `coj --help` to discover available functionality, then inspect `coj <command> --help` for commands relevant to the work. Prefer `coj` over manually reproducing workflows it already supports.
  * Do not exhaustively inspect unrelated commands.
* Use `coj git` for mutating Git operations. For unsupported commands, request permission from the maintainer to use git directly.
* Git stash is intentionally unsupported because it is repository-global. Use branches instead of stashing.
* `dev`, `main`, and `master` are protected regardless of ASCII case. Read them freely, but do not
  mutate them or switch an agent worktree onto them through the ordinary Git path.
* The worktree containing the current CWD is the workspace boundary. Do not access or modify
  another worktree outside that boundary. 
* coj is a cooperative guardrail, not a hostile-process security sandbox. Do not bypass it
  with raw mutating Git, direct `.git` edits, environment overrides, aliases, alternate Git
  executables, shell tricks, or other workarounds. 
* `dev` is the integration branch
* Begin a new task with `coj prepare-worktree` in the current Git worktree. 
  The maintainer installs/updates `coj` and manages PATH; agents invoke it by name from PATH. If it cannot be found or launched, halt and report the problem so the maintainer can fix it.
* After worktree preparation, build only affected targets and execute relevant CTest labels.
  Use `ctest --test-dir out/build/native -L <subsystem> -LE "soak|compile-contract"` for the
  fast loop. Include the applicable expensive categories once for final validation.
  `native-simulation-tests` is the ordinary-only workflow; `native-simulation-full-tests`
  adds the soak for final simulation validation. `native-tests` includes native soak/compile
  contracts but excludes standalone developer-tool tests.
* Perform feature work on dedicated feature branches; `coj git switch -c feature/<task> dev`
  creates one. The `feature/` prefix is a convention, not an ownership requirement.
* Do not bypass instructions here unless explicitly told to
* Use the CMake workflows described in [Builds](#builds).
* **Fast default:** 
  * Understand the task
  * Finish the coherent implementation and required cleanup
  * Format and review it
  * Run the smallest useful native/focused validation
  * Fix with focused checks then report ready
  * Only build and run what is needed.
* Use stock Codex and the cooperative [jobs board](tools/jobserver/README.md). Cheap work needs no
  ticket. Before heavyweight builds/tests, run `coj jobs request shared "operation"`;
  use `exclusive` for benchmarks or work requiring a quiet machine. Check the returned ID with
  `coj jobs check <id>` until Ready. Call `jobs start <id>` immediately before the ordinary
  command and `jobs end <id>` immediately when it returns, including failures. Cancel unused
  queued/ready tickets with `jobs cancel <id>`. These are all `coj jobs` commands.
  Inspect `jobs status`; never jump a queued exclusive ticket.
  This is voluntary coordination: tools do not enforce it. Follow the protocol and report missing tools.
* Run light, relevant tests after implementation and expensive relevant gates once against
  the final candidate. Tooling/native work must not build Unreal without a dependency reason.
* If coj is broken, report it once and follow an explicit maintainer instruction for any
  minimal alternative; do not repeatedly retry or deliberately bypass it.
* Use one simple shell command per tool invocation for routine agent work. Do not combine commands with `;`, `&&`, `||`, pipelines, or script blocks, even when each command is individually approved or cheap.
  * Run inspections as separate tool calls and read their output directly. Independent calls may be batched through the tool API, without combining their shell command strings.
  * Cheap inspections need no ticket. Request tickets separately from the commands they coordinate.
  * Do not redirect command output to log files. Existing build/test scripts may still be invoked normally.
* Make commits for each discrete chunk of work as you work. Use good judgement.
* Try to avoid making just one commit for all the work
* Static analysis is not part of normal feature validation. Run it only when explicitly requested or when modifying static-analysis tooling.

# Builds

* CMake is used to drive all builds, including UBT
* Before editing CMake files, read [CMake style](cmake/STYLE.md).
* Set `IOJ_ROOT` to the absolute base directory for shared tools and temporary files.
  Stable tools are maintainer-installed under `%IOJ_ROOT%\tools\<ToolName>\bin`
  and invoked from PATH.
  Agents never auto-install/update tools or construct local fallbacks.
  Report a missing command; use `--version` for manual source/install comparison when needed.
  CMake builds revision-local game-package-tools, native-binary-tools, and
  benchmark-tools privately on demand.
* Windows tools use `%IOJ_ROOT%\tmp\<worktree-name>` for temporary files. Allow that directory in
  the agent sandbox; do not grant access to the user profile to resolve temporary-path failures.
* For final integration, only build and test what your work has affected
* Keep benchmarks short; Not more than 3 minutes total
* For native simulation and fighter revision comparisons, use `coj benchmark compare
  --baseline <ref> [--candidate <ref>]` rather than ad-hoc worktree or analysis scripts.
  This command manages its own detached inputs under this workspace's `.local/benchmarks/wt/`.
  Omit `--candidate` to include local edits. Use `--prepare-only` under a shared ticket, then
  the printed worktree paths with `--skip-build` under an exclusive ticket; see `docs/benchmarks.md`.
* For probe-level performance work, use `coj benchmark tracy-report --trace <capture.tracy>`
  with a relevant `--filter` and steady-state time window. Compare probe distributions as well as
  full-tick timings. Keep raw traces/exports out of agent context; see `docs/benchmarks.md`.
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
* Keep README.md files concise and user-oriented. Put detailed usage instructions in local READMEs
  or local `docs/` directories, and architecture information in local `ARCHITECTURE.md` files.
* For contention other than jobs-board tickets, treat obvious temporary contention for shared resources as a wait condition: do not repeatedly retry across consecutive turns; sleep within the shell/tool invocation before retrying with 10s, then 30s, then 60s backoff (capped at 60s). Investigate and report the failure if persists unreasonably.
* When replacing an experimental architecture or workflow on a feature branch, remove the superseded path
  completely unless the maintainer requests compatibility. Feature branches are the rollback
  mechanism. Keep one canonical path: no obsolete implementations, compatibility wrappers,
  fallback execution, duplicate configuration, or alternate workflows "just in case".
* When explicitly granted autonomy, use judgement to resolve reasonable ambiguities while keeping scope controlled.
* Store local development roadmaps under `.local/plans/` and disposable session hand-offs under `.local/handoffs/`; never commit them.
* Use common tools like `clang-tidy` from PATH, not from explicit file paths.

Do not say “almost done”, “virtually done”, “nearly finished”, or give percentage-style completion estimates unless all required validation steps are already known and enumerated. When reporting progress, explicitly separate:
- implementation complete/incomplete
- tests complete/incomplete
- static analysis: not yet due / complete / failed
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
* State non-obvious API preconditions and missing-result semantics at the declaration. Treat
  impossible internal inputs as contract violations, with assertions at the responsible boundary;
  do not silently convert them into null/missing results. Handle legitimate optional sentinels
  explicitly before entering stricter bulk operations. Reuse phase guarantees instead of repeating
  validity checks in inner loops. Tests must respect these contracts except when explicitly testing
  invariant detection; prefer normal commands and events for simulation scenarios.
* Log warnings/errors when null checks fail rather than returning silently.
* Visually separate blocks of related code with whitespace, including logical phases within
  functions and groups of declarations or data. Arrange long functions so their main control
  flow is easy to scan, while keeping tightly coupled statements together.
* Add brief comments to potentially hard-to-read code using the present imperative mood
  (for example, "Preserve the original encoding when replacing the setting."). Explain intent
  or non-obvious constraints; do not merely restate the code.
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

* Format changed C++ files with `coj format --changed`
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
* Keep all Rust test implementations, fixtures, and test helpers in each crate's `tests/`
  directory, not inline or in files under `src/`. Unit tests that need private access may use
  `#[cfg(test)]` and `#[path = "../tests/<module>/mod.rs"]` declarations in source modules.
* Use CQTest/Unreal automation tests for code with engine/editor dependencies.
* Register Unreal Automation benchmarks under the top-level `SandboxBenchmarks` category.
* Prefer putting tests in native where possible
* Prefer running native tests as part of the normal flow
* Leave Unreal tests until the end of a task
