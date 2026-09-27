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

`ioj-no-pair` and `ioj-no-tuple` share semantic type recognition and diagnostic ownership
in `lib/ForbiddenStdType.cpp`. Canonical Clang types remove aliases and cv qualification;
the standard class-template declarations, including inline standard namespaces, identify
pair and tuple. References, pointers, arrays, function signatures, template type arguments
and argument packs are inspected recursively. Standard templates' defaulted type-policy
parameters (allocators, comparators, deleters) and standard implementation templates with
C++ reserved identifiers are opaque. This keeps map/iterator implementation types outside
the policy while still checking actual pair-valued insertion results and user-selected
value arguments such as `vector<pair<...>>` or `map<Key, pair<...>>`. The default-parameter
boundary uses the template declaration, so explicitly supplying the default produces the
same classification as omitting it. Unrelated records are not searched for
hidden members or bases. Unresolved dependent types are not guessed, and a bare template
argument such as `Holder<std::pair>` is not itself a pair specialization.

Variables (including parameters, static members and structured-binding backing objects),
fields, function returns and aliases each own one diagnostic per forbidden family.
An alias definition and a subsequent variable using it are independent declarations and
each warn once. Neither nested type arguments nor desugaring layers multiply warnings.
Function-pointer declarations also own parameters nested in their prototype; a function's
actual parameters remain independent of its return type.

Source calls, construction, explicit casts and references whose result is directly pair
or tuple (rather than a containing wrapper) are checked
outside diagnosed declarations. An enclosing forbidden-valued expression owns its nested
expressions. Initializers belong to their declaration; return expressions belong to a
diagnosed function return type. Ownership does not cross a nested callable body. Suppressed
owners still own their expressions, so normal `NOLINT` does not uncover duplicate warnings.

Factories, including arbitrary project functions, are recognized by semantic result type,
never by function name. `auto`, `make_pair`, `make_tuple`, `tie`, `forward_as_tuple` and
`tuple_cat` therefore follow the same rule. Structured bindings backed by pair/tuple warn;
user aggregates and other tuple-protocol types remain clean. The same main-file ownership
boundary as the view checks applies, and no fix-its are supplied.
