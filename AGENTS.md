Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* The orchestrator coordinates the main simulation tick and major gameplay systems.
* Entity data is largely stored in cache-friendly arrays / SOA structures rather than represented entirely by Actors.
* UI is a presentation layer over simulation state and must not own gameplay logic.
* Determinism, debuggability, simple control flow, and performance are important.
* For new code that does not require Unreal Engine, prefer designing it as a standalone component
  under `native/` and integrate it with Unreal through a thin adapter.
* External standalone developer tools may live under `tools/`.

# Builds

* A Windows-only CMake 4.3+/Ninja layer at the repository root invokes UnrealBuildTool through `RunUBT.bat`. `.Target.cs`, `.Build.cs`, and UBT remain authoritative.
* Use the CMake layer for builds; do not invoke UBT, `RunUBT.bat`, or `Build.bat` directly.
* CMake serializes Unreal builds that share an engine checkout. Do not overlap a CMake Unreal build with Visual Studio, Live Coding, or another Unreal build launched outside CMake because those paths do not participate in the CMake lock.
* CMake also coordinates expensive work and benchmarks across worktrees through a per-user machine activity gate under `%TEMP%/SandboxUnrealBuild/activity/v1`. Requests are FIFO: standard build/test work may overlap unless an older benchmark is waiting or running, while benchmarks wait for every older request and run exclusively. Use `get-machine-activity-state` or the `machine-activity-status` target to inspect it.
* Preferred build: `cmake --workflow --preset debug-game`.
* Targets: `editor`, `game`, `core-tests`, `native-tests`, `dev-core`, `resave-assets`, and `generate-project-files`.
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
  * all suites: `cmake --workflow --preset debug-game-tests`
  * unit suites: `cmake --workflow --preset debug-game-unit-tests`
  * level tests after building: `ctest --preset debug-game-level-tests`
* Before starting a timed benchmark, complete all build and setup work, tell the user that the benchmark is ready, and wait for confirmation so they can stop competing work. Dry runs and correctness tests do not require this pause.
* Run only the benchmark subset needed to answer the current question. Do not run a comprehensive benchmark matrix by default; reserve it for explicitly requested broad validation or when every dimension is materially affected.
* For repeated Unreal level benchmark samples, run the iterations within one editor process rather than launching the editor once per sample. For revision comparisons, group each revision's samples into as few editor launches as practical.

# Agent Behaviour

* Resolve ambiguity explicitly rather than guessing. Ask concise, grouped questions when requirements, ownership, architecture, naming, scope, or behaviour are materially unclear.
* Use targeted repository inspection to establish implementation facts. Do not infer user preferences from the repository or perform broad exploration by default.
* Do not inspect Unreal Engine source unless needed to resolve an engine/API behaviour question.
* Once enough context exists, implement rather than continuing exploration.
* Prefer the smallest coherent change that fully implements the requested design.
* Prefer reusable repository scripts over feeding inline Python code to the interpreter.
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
