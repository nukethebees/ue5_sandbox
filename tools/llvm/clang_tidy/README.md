# IOJ clang-tidy checks

These checks are statically linked into clang-tidy. Build and test them using the
[LLVM checker development workflow](../README.md#checker-development).
Simulation opts into policy through `native/simulation/.clang-tidy`.

## View construction

`ioj-loop-view-construction` diagnoses view construction in repeated portions of
classic for, range-for, while and do/while loops. Classic-for initialization and
range setup run once and are excluded, unless nested in another loop. Nested
lambda bodies and local-class methods start a new lexical context; loops inside
those bodies are still checked. Lambda capture initializers belong to the enclosing
context. No interprocedural execution analysis is attempted, including for immediately
invoked lambdas. Unevaluated expressions and copy/move transport are excluded.

`lib/SimulationPolicy.cpp` defines view identity using canonical record/template
declarations: `std::span`, compact SoA views deriving from
`ml::soa_storage_detail::CompactViewState`, vector/countdown templates and an explicit
set of legacy SoA, health, borrowed read and collision view records. Extend that set
intentionally for new families. Aliases and cv/ref qualification do not hide identity.
Owning containers and value snapshots such as `PlayerReadView` are not views.

Policy diagnostics follow the existing translation-unit audit boundary: source
spelled in the main file, excluding macros and system headers. Included generated,
standard-library, LLVM, Unreal and third-party implementation bodies are excluded,
even with an expanded header filter. Normal clang-tidy suppression applies; no fix-its
are supplied.
