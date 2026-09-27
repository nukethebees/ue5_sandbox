# Simulation policy semantics

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

`ioj-loop-view-accessor-call` uses the same type classifier and lexical boundary for
member, free, function-pointer and function-object calls. Names do not affect the
decision; results returned by reference also qualify. A function returning a view is
owned by this check; explicit constructors and aggregate initialization belong to the
construction check. Braced `auto` initialization of a call's result, implicit omitted
aggregate fields and copy/move transport do not add construction diagnostics. An
explicit construction followed by a slicing call contains two distinct operations.

The policy deliberately does not prove loop invariance. A view into storage that grows
or is replaced each tick cannot be hoisted safely. Per-cell/per-entity subranges and
tests that deliberately grow storage one row at a time also require fresh views.
Keep these exceptions local with a reason and ordinary `NOLINT` comments. Stable
columns should still be hoisted out of those loops. No accessor-name exceptions or
cost/optimizer heuristics are used. String views and arbitrary standard range adapters
are outside the initial simulation-data view family set.

`ioj-no-pair` and `ioj-no-tuple` share only explicit type-location recognition in
`lib/ExplicitStdType.cpp`. Template declaration identity (including inline standard
namespaces) distinguishes the standard templates from user types. Both written
template specializations and class template argument deduction are covered, including
cv-qualified types, locals, members, parameters, returns, alias definitions, nested
type arguments and temporaries. Uses of an alias do not repeat the diagnostic on its
definition. Deduced `auto` results, factory calls such as `make_pair`/`make_tuple`, and
structured-binding/tuple-protocol machinery are not explicit type spellings and are
not diagnosed. Template-template arguments are recognized separately from type
locations. A bare using-declaration is not a type use; subsequent written
specializations of the imported template are diagnosed normally.
