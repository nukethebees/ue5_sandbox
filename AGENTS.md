Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* The orchestrator coordinates the main simulation tick and major gameplay systems.
* Entity data is largely stored in cache-friendly arrays / SOA structures rather than represented entirely by Actors.
* UI is a presentation layer over simulation state and must not own gameplay logic.
* Determinism, debuggability, simple control flow, and performance are important.

# Builds

* A Windows-only CMake 4.3+/Ninja layer at the repository root invokes UnrealBuildTool. `.Target.cs`, `.Build.cs`, and UBT remain authoritative.
* Use the CMake layer for builds; do not invoke UBT or `Build.bat` directly.
* Preferred build: `cmake --workflow --preset debug-game`.
* Targets: `editor`, `game`, `core-tests`, `native-tests`, `dev-core`, `resave-assets`, and `generate-project-files`.
* Regenerate project files after changes to modules/plugins, `.Build.cs`, `.Target.cs`, or project/module definitions. For larger tasks, do this once after the full change rather than after intermediate edits.
* Tests:
  * all suites: `cmake --workflow --preset debug-game-tests`
  * unit suites: `cmake --workflow --preset debug-game-unit-tests`
  * level tests after building: `ctest --preset debug-game-level-tests`
* Before starting a timed benchmark, complete all build and setup work, tell the user that the benchmark is ready, and wait for confirmation so they can stop competing work. Dry runs and correctness tests do not require this pause.
* Run only the benchmark subset needed to answer the current question. Do not run a comprehensive benchmark matrix by default; reserve it for explicitly requested broad validation or when every dimension is materially affected.

# Agent Behaviour

* Resolve ambiguity explicitly rather than guessing. Ask concise, grouped questions when requirements, ownership, architecture, naming, scope, or behaviour are materially unclear.
* Use targeted repository inspection to establish implementation facts. Do not infer user preferences from the repository or perform broad exploration by default.
* Do not inspect Unreal Engine source unless needed to resolve an engine/API behaviour question.
* Once enough context exists, implement rather than continuing exploration.
* Prefer the smallest coherent change that fully implements the requested design.
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
* In large classes with many member functions, group declarations and definitions by category using this banner style:
  ```cpp
  /* **************************************** */
  // Category
  /* **************************************** */
  ```
* In `.cpp` category groups, do not put blank lines between adjacent member-function definitions. Separate categories with blank lines around their banners.
* When returning `std::expected`, prefer `std::in_place` / `std::unexpect` when they avoid unnecessary copies or moves.
* For UObject types in a dedicated plugin and namespace, prefer concise names.
* Avoid anonymous-namespace constants in `.cpp` files because Unreal unity builds can merge translation units. Prefer `inline static constexpr` members or `inline constexpr` constants in a specific named namespace.

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
