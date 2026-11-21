Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy C++ space combat game with heavy SoA use. Prefer simple, explicit systems.
* Prefer `native/` for faster builds and tests until Unreal integration is needed.
  Standalone developer tools may live under `tools/`.

# Feature Workflow

* Follow these instructions unless explicitly told otherwise. Assume one active session.
  Keep worktree access within this workspace. Create additional worktrees or delegate only when requested.
* Start tasks with `coj --help`, then read the relevant subcommand help. Use supported workflows.
  Run `coj prepare-worktree` once per fresh LLM session, never again for follow-ups or compaction.
* Invoke `coj` by name. Ask before installing/updating it or changing PATH. If missing or
  unlaunchable, halt and report it. If broken, report once. Use a minimal alternative only on
  explicit maintainer instruction. Never bypass guardrails through raw Git, `.git` edits,
  environment overrides, aliases, alternate executables, shell tricks, or other workarounds.
* Use `coj git` for Git mutations. Ask before using unsupported Git commands directly.
  Use branches in place of repository-global stash. The integration branch is `dev`.
  Treat `dev`, `main`, and `master` as read-only through ordinary Git, regardless of ASCII case.
  Never switch an agent worktree onto them through that path.
* Continue related work on its feature branch. For independent work, use
  `coj git switch -c feature/<task> <base>`. Base on `dev` unless unintegrated changes are
  required. The `feature/` prefix is a naming convention.
* For engineering tasks, inspect and plan, finish implementation, deletions, tests and docs,
  then review and format the complete diff before building and testing. Code generation needed
  for edits is allowed. Collect validation failures, batch fixes, and rerun affected checks.
  Expand validation only for a concrete reason. Report ready only after required checks pass,
  identifying any failures explicitly accepted by the maintainer.
* Use one simple shell command per tool invocation. Batch independent calls through the tool API.
  Never combine routine commands with `;`, `&&`, `||`, pipelines, or script blocks, even if
  approved. Read output directly without redirecting it to logs. Existing build/test scripts may run normally.

## Review and commits

* When reporting line diffs, give additions, deletions and net change separately for handwritten files and generated output.
* Leave your changes **unstaged** for review, including after validation. Stage changes only
  when the maintainer asks to stage or commit them. Commit only on an explicit commit request.
* When committing, use `coj git` and stage one small, logical change at a time. Keep required
  caller updates, generated outputs, tests and directly related docs with their implementation.
  Split independently reviewable or revertible work, including unrelated refactors, docs and
  workflow changes. Review each staged diff before committing and report every commit hash.

# Builds

* Drive all builds, including UBT, through CMake. Read [CMake style](cmake/STYLE.md) before editing CMake files.
* After worktree preparation, build affected targets and run relevant CTest labels. Fast loop:
  `ctest --test-dir out/build/native -L <subsystem> -LE "soak|compile-contract"`.
  Run relevant light checks after implementation and expensive categories once on the final
  candidate. Integration does not rerun validation. Build Unreal for tooling/native work only
  when a dependency requires it.
* `native-simulation-tests` runs the simulation suite. `native-tests` includes native soak and
  compile contracts and excludes standalone developer tools.
* Developer-tool tests are opt-in by scope: run them for changes to tools/tests, their directly
  consumed interfaces, protocols, formats or configuration, shared build/tool infrastructure,
  or tool diagnosis. Use focused tests for affected dependencies and
  `cmake --workflow --preset tool-tests` for shared or uncertain tool infrastructure.
* Never overlap builds against the same build directory or shared Unreal output. Investigate
  lock or resource conflicts before retrying. Use native unity presets (default batch size 8)
  and preserve target-specific exclusions. Use non-unity builds for include isolation or unity
  failures. Keep Unreal's configured unity policy unless measuring it separately.
* Set `IOJ_ROOT` to the absolute shared tools/temp root. Stable tools live under
  `%IOJ_ROOT%\tools\<ToolName>\bin`. Invoke tools, including `clang-tidy`, from PATH.
  Obtain maintainer approval before installing or updating any tool unless already authorised
  for the task. Report missing commands. Do not create local fallbacks. Use `--version` for manual
  source/install comparisons. CMake privately builds revision-local game-package-tools,
  native-binary-tools and benchmark-tools on demand.
* Use `%IOJ_ROOT%\tmp\<worktree-name>` for Windows temporary files. Allow that directory in
  the sandbox without granting user-profile access for temp-path failures.
* Limit benchmarks to three minutes total and avoid competing heavy workloads. Compare native
  simulation/fighter revisions with `coj benchmark compare --baseline <ref> [--candidate <ref>]`.
  Omit `--candidate` to include local edits. Run `--prepare-only`, then use its printed detached
  worktree paths under `.local/benchmarks/wt/` with `--skip-build`. Do not use ad-hoc scripts.
* For probe timings, use `coj benchmark tracy-report --trace <capture.tracy>` with a relevant
  `--filter` and steady-state window. Compare probe distributions and full-tick timings. Keep raw
  traces/exports out of agent context. See [benchmarks](docs/benchmarks.md).

# Agent Behaviour

* Inspect relevant code to establish facts. Ask when unclear and implement once context is
  sufficient. When granted autonomy, resolve reasonable ambiguities and keep scope controlled.
* Never change architecture autonomously. When a serious issue, conflicting requirement or
  proposed workaround calls the design into question, stop and explain the concrete problem.
  Ask as many questions as needed and agree on the approach with the maintainer before
  continuing. Suggest out-of-scope improvements for approval. Avoid unrequested guards,
  fallbacks and workarounds.
* Use progress updates for status and answer maintainer questions in the final response.
  Report changes, checks and unresolved issues. Mention static analysis, sanitizers or Unreal
  validation only when requested or relevant.
* Use “almost done”, “virtually done”, “nearly finished” or percentage estimates only when all
  required validation steps are known and enumerated. Retract estimates if new issues require
  more builds/tests or invalidate validation.
* Keep READMEs concise and user-oriented. Put detailed usage in local READMEs or `docs/`,
  architecture in local `ARCHITECTURE.md`, local roadmaps in `.local/plans/` and disposable
  hand-offs in `.local/handoffs/`. Never commit roadmaps or hand-offs from those directories.
* When replacing experimental architecture or workflows on a feature branch, remove the old
  path unless the maintainer requests compatibility. Use the branch for rollback. Remove
  obsolete implementations, wrappers, fallback execution and duplicate configuration. Avoid
  speculative alternatives.

# Coding Style

* For classes with trailing-underscore data members, name getters without a `get_` prefix.
  Preserve names required by external interfaces and getters whose unprefixed names conflict
  with existing members.
* Use Unreal Engine C++, `snake_case` functions/variables, `TitleCase` types, east const, and braces.
  Do not prefix booleans with `b_`. Prefer `auto` when the type is obvious and simple C++ over template metaprogramming.
* Declare one variable/data member per declaration and line; never combine declarators.
* Order handwritten and generated type declarations: aliases and constexpr constants (with
  supporting nested types), constructors, destructors, copy/move special members, operators,
  ordinary member functions, then data members. Put static factories with ordinary functions.
  Keep data members last and separate distinct groups with blank lines.
* Put forward declarations before definitions, separated by a blank line.
* Keep headers concise. Comment declarations only for non-obvious contracts names/types cannot
  express; put implementation explanations in source files. Never restate routine declarations,
  members, or code in comments.
* Prefer SoA for related performance-sensitive collections. Save loop bounds as const locals.
* Name repeated long expressions with local values/views. Keep views within storage lifetime
  and never retain them across invalidating mutations.
* Assert impossible internal inputs at the responsible boundary; never silently turn them into
  null/missing results.
  Handle legitimate optional sentinels before stricter bulk operations. Reuse phase guarantees
  instead of repeating inner-loop validation. Tests must respect contracts unless testing
  invariant detection; prefer normal commands/events for simulation scenarios.
* Log warnings/errors for failed null checks rather than returning silently.
* Separate related code blocks with whitespace. Make long-function control flow easy to scan
  while keeping coupled statements together. In non-trivial functions, separate distinct guards,
  validation, setup, state transitions, loops, and final publication/returns with blank lines.
* Group large classes' member declarations and definitions by category using:
  ```cpp
  /* **************************************** */
  // Category
  /* **************************************** */
  ```
  Order categories by lifecycle/control flow; align header/source order where practical.
  In `.cpp` category groups, omit blank lines between adjacent function definitions; separate
  categories with blank lines around banners.
* Prefer `std::in_place`/`std::unexpect` for `std::expected` returns when they avoid copies/moves.
* Prefer concise UObject names within dedicated plugins/namespaces.
* Avoid anonymous-namespace `.cpp` constants: Unreal unity builds can merge translation units.
  Prefer `static constexpr` members or `inline constexpr` constants in a specific named namespace.

# Code Reuse

* Put generated production code beside handwritten code in normal source/include trees, never
  production `generated/` directories. Generated test fixtures may use them.
* Reuse existing code when it performs the task.
* Define closed language keyword enums in Lispb; generate serialized names and parsing. Keep
  contextual grammar restrictions explicit. Do not create enums for open identifiers or property
  names already owned by record metadata.
* Keep generated code simple and debuggable. Share repetitive typed SoA operations through
  existing column packs when this reduces total code; generate schema-specific behavior explicitly.
  Avoid extra type-description systems or elaborate template frameworks.
* Emit compiler-owned C++ symbols through existing typed CppIR and the scope-aware renderer.
  Never bypass resolution with qualified names embedded in generated text or local namespace
  stripping. Reserve opaque text for authored C++ and syntax the IR cannot express.

# Formatting

* Format changed C++ with `coj format --changed`; do not invoke `clang-format` directly for normal work.
* For long or visually crowded braced initializer lists, use one initializer per line and a
  trailing comma. Keep short lists compact; judge readability rather than using a fixed item count.

# UI

* Use `BindWidget` for UPROPERTY widgets.
* Use `meta=(GeneratorRoot)` for generated UMG widgets whose root is a panel.
* Consider the repository's Slate DSL where useful. Keep gameplay logic out of UI widgets.

# Testing

* Run static analysis only when explicitly requested or modifying its tooling. Run AddressSanitizer (ASAN)
  builds/tests only when explicitly requested.
* Do not add data members to production classes solely to support tests or benchmarks without
  explicit maintainer approval. Prefer APIs that serve both production and validation needs;
  keep test-specific state in fixtures and pass inputs or output collectors explicitly.
* Create tests only when explicitly asked.
* Keep automated tests/fixtures separate from production, in `tests/` or dedicated Unreal test modules.
* Put all Rust test implementations, fixtures, and helpers in each crate's `tests/`, never inline
  or under `src/`. Private-access unit tests may use `#[cfg(test)]` and
  `#[path = "../tests/<module>/mod.rs"]` declarations in source modules.
* Use CQTest/Unreal automation for engine/editor dependencies. Register Unreal Automation
  benchmarks under top-level `SandboxBenchmarks`.
* Prefer native tests and run them in the normal flow. Leave Unreal tests until the task's end.
