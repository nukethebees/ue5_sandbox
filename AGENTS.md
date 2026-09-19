Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* The orchestrator coordinates the main simulation tick and major gameplay systems.
* Entity data is largely stored in cache-friendly arrays / SOA structures rather than represented entirely by Actors.
* UI is a presentation layer over simulation state and must not own gameplay logic.
* Determinism, debuggability, simple control flow, and performance are important.
* Prefer implementing work under `native/` until Unreal Engine integration is needed, then add a
  thin adapter after the native implementation is substantially settled. Local native validation is
  the fast path and avoids contention on shared Unreal engine resources.
* External standalone developer tools may live under `tools/`.

# Feature Workflow

* Use the canonical absolute `agent-git` executable for supported Git mutations. Raw Git remains
  acceptable for read-only inspection; unsupported mutations require the normal human-approval
  route rather than bypassing `agent-git` or using a repository-built copy.
* The unconditional rule applies only to the canonical executable. It does not cover its installer,
  raw mutating Git, edits to the canonical installation, trust manifest, or isolation configuration,
  or direct changes to Git administrative data.
* `dev` is the integration branch. Perform feature work on dedicated feature branches in separate
  worktrees rather than directly on `dev`; multiple agents may work concurrently in their own
  worktrees.
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
* Prefer multiple coherent commits for substantive work when there are natural implementation
  stages.
* A feature is **ready for integration** when its implementation is formatted, reviewed, and given
  credible light/focused validation, and its commits are coherent. Do not repeatedly rebase merely
  because `dev` advanced; rebase during development only for a specific known dependency. Report
  readiness and wait for the user's explicit authorization without acquiring a reservation.
* After the user explicitly authorizes integration, run `integrate-feature` from the feature
  worktree. This queues fairly for the exclusive `integration/dev` jobserver resource; ordinary
  feature work and unrelated jobserver resources remain concurrent. Do not bypass this queue for a
  normal agent merge into `dev`.
* The granted reservation begins one final integration transaction:
  1. Record the exact current `dev` HEAD as `integration_base_sha`, then rebase the feature once
     onto that exact commit. If conflicts occur, abort and release the reservation; resolve them
     carefully outside the queue and requeue the ready feature.
  2. Review the rebased range/diff for accidental conflict-resolution changes or lost work and
     complete the tool's bounded review checkpoint.
  3. Run the applicable final merge-ready gate from [Builds](#builds), then build every buildable
     target with `cmake --workflow --preset development`. The optimized `NDEBUG` build remains
     mandatory even when the test gate uses DebugGame.
  4. Immediately before merging, verify that current `dev` still equals
     `integration_base_sha`. If it differs, treat this as an integration-protocol violation: abort,
     report both SHAs, preserve the feature branch, release the reservation, and do not rebase or
     restart the validation loop automatically.
  5. If validation passed and `dev` is unchanged, merge immediately and perform the normal
     worktree/feature-branch cleanup before releasing the reservation. The user's authorization to
     enter the queue also authorizes this merge; do not stop after validation to ask a second time.
* Rebase conflicts or failed final validation abort the transaction and release its reservation.
  Resolve or fix the feature outside the queue with focused validation, then requeue it when ready;
  never use the reservation for open-ended debugging or automatically retry an entire integration.
  The connection-owned lease releases on every normal, failure, cancellation, or disconnect path,
  and its local process tree is terminated if the lease holder exits.
* After a feature branch has been successfully merged into `dev`, return its worktree to its
  normal persistent branch when one exists. Infer that branch from the worktree directory name
  only when a matching branch exists; for example, worktrees named `dev1` through `dev10` normally
  return to persistent branches with the same names. Never delete a persistent `devN` branch.
* Once the worktree is no longer on the feature branch, delete that feature branch only if it is
  fully merged into `dev` and the user has not asked to keep it. Use safe deletion semantics
  (`git branch -d`), never force-delete an unmerged branch; leave an unmerged branch intact and
  report that cleanup was skipped.
* A branch task is not complete until its original plan and all approved amendments are complete.
  Before integration authorization, report a completed substantive feature as ready for
  integration rather than done. After authorization, it is complete only when the transaction and
  any required cleanup finish.

# Builds

* A Windows-only CMake 4.3+/Ninja layer at the repository root invokes UnrealBuildTool through `RunUBT.bat`. `.Target.cs`, `.Build.cs`, and UBT remain authoritative.
* Use the CMake layer for builds; do not invoke UBT, `RunUBT.bat`, or `Build.bat` directly.
* `ctools` builds and stages standalone C# developer-tool executables under `tools/bin`. Before a
  workflow that invokes one, run `ctools` if its executable is absent. Unreal workflows require
  `UnrealBuildTools.exe`; formatting workflows require `CodeFormatTools.exe`. Native-only
  workflows do not require this preflight: their mimalloc validation builds its configuration-local
  `NativeBinaryTools` host dependency on demand.
* CMake coordinates Unreal work through a canonical engine read/write gate: builds, UAT packaging,
  and interactive managed editor launches acquire it exclusively; unattended managed editor tests
  and commandlets acquire it shared for their full process-tree lifetime. Shared editor readers must
  use `-unattended` so startup cannot prompt to compile modules. Regenerate CMake commands in every
  worktree and reload PowerShell helpers after coordination changes. Manually launched editors,
  Visual Studio builds, Live Coding, and UBT launched outside CMake do not participate; do not
  overlap them with managed Unreal builds.
* UBT is authoritative for BuildIds, target receipts, and module manifests. Do not inspect, edit,
  copy, or synchronize BuildIds in project tooling. Normal project builds do not use
  `-NoEngineChanges`: project-owned runtime dependencies such as `SandboxTracyClient.dll` are
  legitimately staged into the Editor target's engine output directory. The exclusive engine gate
  serializes those writes.
* A canonical per-user jobserver coordinates expensive work across worktrees. Ordinary work shares the machine resource; benchmarks wait for older work to drain and then run exclusively. Use `get-jobserver-state` or the `jobserver-status` target to inspect running and queued jobs. Continue to use repository CMake/PowerShell wrappers for coordinated work; do not bypass them merely to customize queue metadata.
* Cheap native validation: build the affected target with
  `cmake --build --preset native --target <target>`, then run the applicable focused native
  workflow. `cmake --workflow --preset native-tests` is the broader native suite. These commands
  do not configure or build Unreal.
* Unreal-facing validation: use the smallest focused Unreal build/test only when the boundary
  itself needs checking, such as module/build definitions, UObject/reflection, adapters/APIs, UI,
  assets, editor integration, or ownership/lifetime behaviour.
* Final integration validation for substantive code, schema, generated-source, build-configuration,
  module, or test changes is `cmake --workflow --preset debug-game-tests`, followed by the required
  `cmake --workflow --preset development`, only inside the authorized reserved transaction after
  its rebase.
* When a generated-code check reports stale outputs, automatically run its matching `generate-*`
  CMake target, review the generated diff, and rerun the failed check.
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
  * If Unreal tests report missing or stale project/plugin modules, explicitly build the relevant
    `editor` target through its CMake preset before rerunning them. Let UBT regenerate its receipts
    and module manifests; never edit or synchronize BuildIds manually.
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
* Keep README.md files concise and user-oriented. Put detailed architecture, data flow, invariants, and implementation notes in an adjacent ARCHITECTURE.md, and link it from the README.
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

# SandboxCore and SOA Reuse

* Inspect `Plugins/SandboxCore` before writing generic container, array, SOA, math/vector, timer/countdown, buffering, or permutation helpers.
* Reuse matching helpers in handwritten code when clarity and performance are preserved; short local duplicates are still duplicates.
* Prefer complete generated SOA operations for row growth, removal, copying, and reordering. Column synchronization is an invariant.
* Keep generated implementations explicit, simple, and easy to debug. Do not consolidate their per-column operations into variadic calls.
* For missing reusable utilities, consider tested SandboxCore additions or general-purpose generated member functions for repeated SOA operations.
* Keep explicit hot algorithms where a shared helper would be less clear or slower.

# Formatting

* Format changed C++ files with `cmake --workflow --preset format-code` once implementation has
  substantially settled.
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
