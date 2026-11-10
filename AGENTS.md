Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game, primarily C++, with heavy SoA usage.
* Prefer simple, explicit systems over speculative abstraction.
* Keep gameplay logic out of UI.
* Prefer `native/` until Unreal integration is needed; local code avoids shared engine contention.
* Standalone developer tools may live under `tools/`.

# Feature Workflow

* Use `coj`, the project workflow CLI. Start tasks with `coj --help`, then inspect relevant
  `coj <command> --help`. Prefer supported workflows over manual equivalents; skip unrelated help.
* Run `coj prepare-worktree` in the current worktree once at the start of a fresh LLM session.
  Do not repeat it for new tasks, follow-ups, context compaction, or delegated agents within
  the same session. Invoke `coj` by name; ask the maintainer before installing/updating it or changing PATH.
  If it cannot launch or is missing,
  halt and report it. If broken, report once; use a minimal alternative only under explicit
  maintainer instruction. Do not repeatedly retry or bypass it.
* Use `coj git` for mutating Git operations. Ask the maintainer before using unsupported Git
  commands directly. Stash is unsupported because it is repository-global; use branches.
* `dev` is the integration branch. `dev`, `main`, and `master` are protected regardless of
  ASCII case: read them freely, but do not mutate them or switch agent worktrees onto them
  through the ordinary Git path.
* Work on dedicated feature branches; create one with `coj git switch -c feature/<task> dev`.
  `feature/` is a convention, not an ownership requirement.
* Treat the current worktree as the workspace boundary. Do not access or modify worktrees outside that boundary.
* Treat `coj` as a cooperative guardrail, not a hostile-process sandbox. Never bypass it with
  raw mutating Git, `.git` edits, environment overrides, aliases, alternate Git executables,
  shell tricks, or other workarounds.
* Follow these instructions unless explicitly told otherwise. Use the [CMake workflows](#builds).
* Use this pipeline for every engineering task: inspect and plan, finish all implementation,
  deletions, test updates and documentation, review and format the complete diff, then build
  and test. Finish delegated edits before starting validation. Do not build or test between
  partial implementation steps merely to check progress; code generation needed to complete
  edits is allowed. After the complete candidate enters validation, collect failures and batch
  the required fixes before repeating affected checks. Do not alternate individual edits with
  rebuilds or retests, or expand validation without a concrete need. Report ready only after
  the required checks pass, and report accepted failures explicitly.
* After worktree preparation, build affected targets and run relevant CTest labels. Fast loop:
  `ctest --test-dir out/build/native -L <subsystem> -LE "soak|compile-contract"`.
  Run applicable expensive categories once against the final candidate.
  `native-simulation-tests` runs ordinary tests; `native-simulation-full-tests` adds the soak.
  `native-tests` includes native soak/compile contracts, excluding standalone developer tools.
* Run light, relevant tests after implementation and expensive relevant gates once on the final
  candidate. Do not build Unreal for tooling/native work without a dependency reason.
* Use one simple shell command per tool invocation for routine work. Never combine commands with `;`,
  `&&`, `||`, pipelines, or script blocks, even if approved or cheap. Inspect through separate
  calls and read output directly; batch independent calls through the tool API, not shell strings.
  Do not redirect output to log files; existing build/test scripts may run normally.
* Commit discrete chunks as you work, using judgement; avoid one commit for everything.
* Run static analysis only when explicitly requested or modifying its tooling, not as normal
  feature validation.

# Builds

* Drive all builds, including UBT, through CMake. Read [CMake style](cmake/STYLE.md) before editing CMake files.
* Use the native unity presets for normal development work; their default batch size is 8.
  Preserve target-specific unity exclusions and use non-unity builds for include-isolation
  checks or unity-specific failures. Keep Unreal's configured unity policy unless measuring it separately.
* Set `IOJ_ROOT` to the absolute shared-tools/temp root. Install stable tools
  under `%IOJ_ROOT%\tools\<ToolName>\bin`; invoke them from PATH. Installing a new tool or updating
  an existing tool is allowed with maintainer approval, including tools not mentioned here.
  Ask before installing or updating unless already authorized in the current task. Do not
  construct local fallbacks. Report missing commands; use `--version` for manual source/install
  comparison when needed. CMake privately builds revision-local game-package-tools,
  native-binary-tools, and benchmark-tools on demand.
* Use `%IOJ_ROOT%\tmp\<worktree-name>` for Windows temporary files. Allow that directory in the
  sandbox; do not grant user-profile access to fix temporary-path failures.
* Build and test only affected work for final integration. Integration does not rerun validation.
* Limit benchmarks to three minutes total.
* Compare native simulation/fighter revisions with
  `coj benchmark compare --baseline <ref> [--candidate <ref>]`, not ad-hoc worktree/analysis scripts.
  It manages detached inputs under this workspace's `.local/benchmarks/wt/`. Omit `--candidate`
  to include local edits. Run `--prepare-only`, then use the printed worktree paths with
  `--skip-build`. See `docs/benchmarks.md`.
* For probe-level performance, use `coj benchmark tracy-report --trace <capture.tracy>` with a
  relevant `--filter` and steady-state window. Compare probe distributions and full-tick timings.
  Keep raw traces/exports out of agent context; see `docs/benchmarks.md`.
* Exclude standalone developer-tool tests from default validation. Run
  `cmake --workflow --preset tool-tests` only for changes affecting tools/tests, their directly
  consumed interfaces/protocols/formats/configuration, shared build/tool infrastructure, or tool
  diagnosis. Do not run them for unrelated native/game/runtime work because a broad command exists.
  Choose focused tests for affected dependencies; use the complete suite for shared/unknown tool
  infrastructure.

# Agent Behaviour

* Avoid overly defensive programming. When you identify a possible risk, design gap, or suspected
  mistake, explain it, suggest options, and ask the maintainer to decide how to handle it.
  Do not assume the intended policy or add unrequested guards, fallbacks, or fixes.
* Establish facts through targeted repository inspection. Do not guess; ask when unclear.
  Once context is sufficient, implement rather than continuing exploration.
* Answer maintainer questions in the final response after completing the work item, where tool
  output cannot bury them. Use progress updates for work status.
* Suggest improvements beyond the requested changes rather than implementing them without asking.
* Keep README.md files concise and user-oriented. Put detailed usage in local READMEs or `docs/`,
  and architecture in local `ARCHITECTURE.md` files.
* For temporary shared-resource contention, wait rather than retrying
  across consecutive turns. Sleep within the shell/tool invocation before retries: 10s, 30s, then
  60s, capped at 60s. Investigate and report unreasonable persistence.
* When replacing experimental architecture/workflows on a feature branch, remove the old path
  completely unless the maintainer requests compatibility. Branches provide rollback. Keep one
  canonical path: no obsolete implementations, compatibility wrappers, fallback execution,
  duplicate configuration, or speculative alternate workflows.
* When granted autonomy, resolve reasonable ambiguities with judgement and keep scope controlled.
* Keep local roadmaps in `.local/plans/` and disposable hand-offs in `.local/handoffs/`; never commit either.
* Invoke common tools such as `clang-tidy` from PATH, not explicit paths.

Do not say "almost done", "virtually done", "nearly finished", or give percentage completion
estimates unless all required validation steps are known and enumerated. Report these separately:

* Implementation: complete/incomplete.
* Tests: complete/incomplete.
* Static analysis: not yet due/complete/failed.
* Sanitizer validation: complete/incomplete.
* Integration/build validation: complete/incomplete.
* Unresolved issues.

Retract earlier completion estimates if new issues invalidate validation or require more builds/tests.

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

* Do not add data members to production classes solely to support tests or benchmarks without
  explicit maintainer approval. Prefer APIs that serve both production and validation needs;
  keep test-specific state in fixtures and pass inputs or output collectors explicitly.
* Run AddressSanitizer (ASAN) builds/tests only when explicitly requested. ASAN is not a default
  validation, readiness, or integration requirement.
* Create tests only when explicitly asked.
* Keep automated tests/fixtures separate from production, in `tests/` or dedicated Unreal test modules.
* Put all Rust test implementations, fixtures, and helpers in each crate's `tests/`, never inline
  or under `src/`. Private-access unit tests may use `#[cfg(test)]` and
  `#[path = "../tests/<module>/mod.rs"]` declarations in source modules.
* Use CQTest/Unreal automation for engine/editor dependencies. Register Unreal Automation
  benchmarks under top-level `SandboxBenchmarks`.
* Prefer native tests and run them in the normal flow. Leave Unreal tests until the task's end.
