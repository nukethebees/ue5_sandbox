# Lispb architecture

Lispb parses constrained declarative input and emits checked source assets; it is not a general Lisp
runtime or an escape hatch for arbitrary C++. Each DSL owns its grammar, semantic validation, and
stable output contract while sharing the parser and generation workflow.

`kernel` describes typed SandboxCore operations and C++ emission profiles. `slate` describes static
Slate trees and host integration. `material` describes material graphs and emits Unreal assets.
Generated outputs remain repository-owned source: modify the DSL input, run the matching CMake
target, review the generated diff, and keep hand-written integration at the boundary.

See [README.md](README.md) for entry points.
