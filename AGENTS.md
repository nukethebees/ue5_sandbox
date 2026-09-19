Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* The orchestrator coordinates the main simulation tick and major gameplay systems.
* Entity data is largely stored in cache-friendly arrays / SOA structures rather than represented entirely by Actors.
* UI is a presentation layer over simulation state and must not own gameplay logic.
* Determinism, debuggability, simple control flow, and performance are important.
* Prefer implementing and testing work under `native/` until Unreal Engine integration is needed,
  then integrate it with Unreal through a thin adapter.
* Use the local native testing infrastructure where possible to keep iteration fast and avoid
  clashes with the shared Unreal build mutex.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* `dev` is the integration branch. Perform feature work on dedicated feature branches in separate
  worktrees rather than directly on `dev`; multiple agents may work concurrently in their own
  worktrees.
* Use the CMake workflows and repository jobserver described in [Builds](#builds) for expensive
  jobs. Do not bypass that coordination or interfere with jobs owned by other worktrees or agents.
* During feature development, run only the smallest relevant build or focused test that directly
  validates the current work. Escalate validation when the change's risk or scope warrants it.
  For trivial non-functional changes such as documentation or comments, do not run tests by
  default; state that tests were not run because the change was trivial, so the user can request
  them.
* Prefer multiple coherent commits for substantive work when there are natural implementation
  stages. Do not repeatedly rebase during ordinary feature development without a concrete reason.
* When preparing a completed feature for integration:
  1. Commit the completed implementation.
  2. Rebase the feature branch onto current `dev` and resolve conflicts carefully.
  3. Review the resulting diff for accidental conflict-resolution changes or lost work.
  4. Run the applicable merge-ready integration gate from [Builds](#builds) on the rebased branch.
  5. Diagnose and fix failures using focused validation where appropriate, then rerun the required
     merge-ready integration gate on the final HEAD.
  6. As a final check before proposing a merge into `dev`, build every buildable target on the
     final rebased HEAD to catch possible compiler errors.
  7. Only report the branch as merge-ready after its final rebased HEAD passes the required gate
     and all buildable targets build successfully. Do not merge it into `dev` without the user's
     explicit permission; ask for that permission once the branch is ready.
* After a feature branch has been successfully merged into `dev`, return its worktree to its
  normal persistent branch when one exists. Infer that branch from the worktree directory name
  only when a matching branch exists; for example, worktrees named `dev1` through `dev10` normally
  return to persistent branches with the same names. Never delete a persistent `devN` branch.
* Once the worktree is no longer on the feature branch, delete that feature branch only if it is
  fully merged into `dev` and the user has not asked to keep it. Use safe deletion semantics
  (`git branch -d`), never force-delete an unmerged branch; leave an unmerged branch intact and
  report that cleanup was skipped.
* A branch task is not complete until its original plan and all approved amendments are complete.
  A substantive task is not complete merely because implementation is finished: it must also
  complete the merge-ready process before being reported as done.

# Builds

* A Windows-only CMake 4.3+/Ninja layer at the repository root invokes UnrealBuildTool through `RunUBT.bat`. `.Target.cs`, `.Build.cs`, and UBT remain authoritative.
* Use the CMake layer for builds; do not invoke UBT, `RunUBT.bat`, or `Build.bat` directly.
* `ctools` builds and stages standalone C# developer-tool executables under `tools/bin`. Before a
  workflow that invokes one, run `ctools` if its executable is absent. Unreal workflows require
  `UnrealBuildTools.exe`; formatting workflows require `CodeFormatTools.exe`. Native-only
  workflows do not require this preflight.
* CMake coordinates Unreal work through a canonical engine read/write gate: builds and UAT packaging acquire it exclusively; managed editor launches, tests, and commandlets acquire it shared for their full process-tree lifetime. Regenerate CMake commands in every worktree and reload PowerShell helpers after coordination changes. Manually launched editors, Visual Studio builds, Live Coding, and UBT launched outside CMake do not participate; do not overlap them with managed Unreal builds.
* A canonical per-user jobserver coordinates expensive work across worktrees. Ordinary work shares the machine resource; benchmarks wait for older work to drain and then run exclusively. Use `get-jobserver-state` or the `jobserver-status` target to inspect running and queued jobs.
* Development validation has three tiers:
  1. Native-only validation is the default while implementing independently buildable code under
     `native/`. Use `cmake --build --preset native --target <target>` and focused native workflows
     such as `native-simulation-tests`; `cmake --workflow --preset native-tests` runs the complete
     native suite. These commands do not configure or build Unreal.
  2. Use the smallest focused Unreal build/test only after native work is substantially settled and
     an Unreal-facing boundary needs checking: module/build definitions, UObject/reflection,
     engine adapters/APIs, UI, assets, editor integration, or ownership/lifetime behavior. A thin
     Unreal adapter alone does not justify rebuilding Unreal after every native implementation edit.
  3. Run the complete DebugGame integration gate only for final merge readiness after rebasing.
* The merge-ready integration gate for substantive code, schema, generated-source,
  build-configuration, module, or test changes is `cmake --workflow --preset debug-game-tests`.
  Follow the [Feature Workflow](#feature-workflow) when applying this gate; do not call the branch
  merge-ready if it fails, even when the failure appears unrelated. When it exposes a focused
  native failure, diagnose and fix it with the smallest relevant native build/test, then rerun one
  final integration gate on the final HEAD.
* Changes that affect benchmark sources or benchmark schemas also require the dedicated benchmark build: configure with `cmake --preset benchmark`, then build with `cmake --build --preset benchmark --target benchmarks`. Run benchmark measurements only when the task requires them.
* Targets: `native-tests`, `native-core-tests`, `native-simulation-tests`, `editor`, `game`,
  `unreal-unit-tests`, `core-tests`, `dev-core`, `resave-assets`, `generate-project-files`, `cook`,
  `cook-incremental`, `stage`, `archive`, `run-staged`, and `verify-package`. Cook targets are
  available from the Development configure preset; stage/archive/run/verify use the current game
  configuration.
* Iterative staged game: `cmake --workflow --preset development-staged-game`.
* Full Development package: `cmake --workflow --preset development-package`.
* Full Shipping package, including the DebugGame test gate and Development cook: `pwsh -NoProfile -File PowerShell/PackageGame.ps1`. Use `-SkipTests` only when the test gate has already completed.
* Link every first-party native CMake target to `sandbox::warnings_as_errors` or a stricter
  project-specific warning interface. The underlying
  `sandbox::warnings` interface requires at least `-Wall -Wextra -Wpedantic` for GNU-style GNU,
  Clang, and AppleClang frontends, and `/W4`; the errors interface adds `-Werror` or `/WX`.
  Treat clang-cl as an MSVC-style frontend and use `/W4 /WX`; do not use `/Wall` or Clang
  `-Wall`, because clang-cl treats that level as `-Weverything`.
* Prefer hierarchical CMake configuration. A subdirectory that owns a library, executable, or
  test suite should define that target in an adjacent `CMakeLists.txt`; parent files should limit
  themselves to `add_subdirectory` calls and shared orchestration where practical.
* Declare CMake targets before their source lists and attach implementation files with
  `target_sources`; do not list sources directly in `add_library` or `add_executable`.
* Regenerate project files after changes to modules/plugins, `.Build.cs`, `.Target.cs`, or project/module definitions. For larger tasks, do this once after the full change rather than after intermediate edits.
* Tests:
  * normal native suite: `cmake --workflow --preset native-tests`
  * focused native suites: `cmake --workflow --preset native-core-tests` or
    `cmake --workflow --preset native-simulation-tests`
  * focused Unreal-enabled unit suites: `cmake --workflow --preset debug-game-unit-tests`;
    this includes an Editor build and is integration validation, not the default native unit path
  * final all-suite integration gate: `cmake --workflow --preset debug-game-tests`
  * level tests after building: `ctest --preset debug-game-level-tests`
  * If Unreal tests report missing project plugin modules, run `cmake --build --preset debug-game --target editor` to repair stale editor-module BuildIds before rerunning them.
* Before starting a timed benchmark, complete all build and setup work, then run it without asking the user for confirmation. Use the repository benchmark targets and scripts so the benchmark acquires exclusive machine access from the jobserver, waits for older work, and runs without build interference. If a benchmark entry point does not request benchmark resources, fix or wrap it before collecting timings. Dry runs and correctness tests do not require benchmark access.
* Run only the benchmark subset needed to answer the current question. Do not run a comprehensive benchmark matrix by default; reserve it for explicitly requested broad validation or when every dimension is materially affected.
* For repeated Unreal level benchmark samples, run the iterations within one editor process rather than launching the editor once per sample. For revision comparisons, group each revision's samples into as few editor launches as practical.

# Agent Behaviour

* Resolve ambiguity explicitly rather than guessing. Ask concise, grouped questions when requirements, ownership, architecture, naming, scope, or behaviour are materially unclear.
* Use targeted repository inspection to establish implementation facts. Do not infer user preferences from the repository or perform broad exploration by default.
* Do not inspect Unreal Engine source unless needed to resolve an engine/API behaviour question.
* Once enough context exists, implement rather than continuing exploration.
* Prefer the smallest coherent change that fully implements the requested design.
* Prefer reusable repository scripts over feeding inline Python code to the interpreter.
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

# SandboxCore and SOA Reuse

* Inspect `Plugins/SandboxCore` before writing generic container, array, SOA, math/vector, timer/countdown, buffering, or permutation helpers.
* Reuse matching helpers in handwritten code when clarity and performance are preserved; short local duplicates are still duplicates.
* Prefer complete generated SOA operations for row growth, removal, copying, and reordering. Column synchronization is an invariant.
* Keep generated implementations explicit, simple, and easy to debug. Do not consolidate their per-column operations into variadic calls.
* For missing reusable utilities, consider tested SandboxCore additions or general-purpose generated member functions for repeated SOA operations.
* Keep explicit hot algorithms where a shared helper would be less clear or slower.

# Formatting

* Format changed C++ files with `cmake --workflow --preset format-code`.
* Do not invoke `clang-format` directly for normal repository work.
* Use `format-all-code` only for explicitly requested repository-wide formatting.

# UI

* Use `BindWidget` for UPROPERTY widgets. Use `BindWidgetOptional` only when the widget is explicitly generated in C++ every time.
* For generated UMG widgets whose root is a panel widget, use `meta=(GeneratorRoot)`.
* When writing Slate layouts, consider using the repository's Slate DSL where its static-tree generation would improve clarity; keep dynamic state and iteration in handwritten code.
* Keep gameplay logic out of UI widgets.

# Testing

* Only create tests when explicitly asked.
* Prefer Catch2 for code without Unreal Engine/editor dependencies. Catch2 targets must not depend on engine/editor modules such as `CoreUObject`, `Engine`, `Slate`, `UMG`, or `UnrealEd`.
* Use CQTest/Unreal automation tests for code with engine/editor dependencies.
* Register Unreal Automation benchmarks under the top-level `SandboxBenchmarks` category.
* Run builds and tests through the repository CMake workflows. Do not launch Unreal or run automation tests outside that flow unless explicitly asked.
* Python scripts may be run when needed; run Pyright on changed Python files.
* For level tests:
  * Prefer `FSoftTestAssertions`.
  * Use `SANDBOX_TESTS_ASSERT_ALL_PASSED` to end assertion stages after soft failures.
  * Simulations must use `TestSimulationDriver` and call `start_simulation`.
  * Record results in `TimeSeriesData` from the end-tick hook and assert against recorded samples rather than live state.
  * Schedule damage/kills through `TestSimulationDriver::timeline` at positive simulation time.
  * Keep each test's data and functions together, separated with the existing banner style.
