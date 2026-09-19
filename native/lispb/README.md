# Lispb code generation

Lispb contains the small declarative DSLs used to generate SandboxCore kernels, Slate layouts, and
Unreal material assets. Use the repository CMake generation targets rather than invoking emitters
directly.

See [Semantic type graph](SEMANTIC_TYPE_GRAPH.md) for the shared resolved model consumed by code
generation and layout tooling. See [ARCHITECTURE.md](ARCHITECTURE.md) for the shared generation
pipeline and the sibling DSL documents for their language-specific contracts.
