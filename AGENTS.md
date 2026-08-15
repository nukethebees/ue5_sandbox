Unreal Engine 5.8 project.

# Project Context

* Simulation-heavy space combat game written primarily in C++.
* Prefer simple, explicit systems over speculative abstraction.
* The orchestrator coordinates the main simulation tick and major gameplay systems.
* Entity data is largely stored in cache-friendly arrays / SOA-style structures rather than represented entirely by Actors.
* UI is a presentation layer over simulation state and should not own gameplay logic.
* Determinism, debuggability, simple control flow, and performance are important.

# Agent Behaviour

* Default to clarification over interpretation.
* If requirements, architecture, ownership, scope, naming, or intended behaviour are ambiguous, ask the user rather than guessing.
* Prefer concise questions over broad repository exploration when the user can provide the missing context directly.
* Ask as many questions as materially improve the plan; group related questions together.
* Use repository inspection to establish implementation facts, not to infer user preferences.
* Keep exploration targeted to directly relevant files and dependencies. Do not launch broad or parallel repository searches by default.
* Do not inspect Unreal Engine source unless necessary to resolve an API or engine-behaviour question.
* Once enough context exists to proceed safely, stop exploring and implement.
* Prefer the smallest change that satisfies the request. Do not perform unrelated refactors.
* If the user says "engage in freedom", "use your judgement", or otherwise grants autonomy, resolve reasonable ambiguities yourself while keeping scope controlled.

# Coding Style

* Unreal Engine C++
* snake_case for functions and variables
* Do not prefix boolean variables with `b_`.
* TitleCase for types
* east const
* always use braces
* prefer auto where the type is obvious
* prefer simple C++ over template metaprogramming
* Prefer SOA layouts for related, performance-sensitive collections.
* save loop bounds as const local variables
* log warnings/errors when null checks fail instead of returning silently
* Group functions by category

# UI Design

* Use BindWidget for UPROPERTY widgets.
* When generating a UMG widget whose root node is a panel widget (for example, `UGridPanel`), use `meta=(GeneratorRoot)`.
* Keep gameplay logic out of UI widgets.

# Testing

* Only create tests when explicitly asked.
* When adding tests, do not compile or run them; ask the user to compile and check them.
* Use `FSoftTestAssertions` as the default assertion mechanism for level-based tests.
* `SANDBOX_TESTS_ASSERT_ALL_PASSED` returns when a soft assertion has failed; use it to end assertion stages instead of adding duplicate failure branches.
* All test levels that use the orchestrator and run a simulation must use `TestSimulationDriver` and call `start_simulation` when the test starts.
* Capture level-test simulation results in `TimeSeriesData` from the end-tick hook; make assertions against the relevant recorded samples rather than live state.
* To avoid time-zero ordering issues, schedule simulation-test damage and kills through `TestSimulationDriver::timeline` at a positive simulation time.
* Group each test's data and functions together, using `/* ------------------------------------------------------------------------------------------ */` banners to separate test-specific sections.
