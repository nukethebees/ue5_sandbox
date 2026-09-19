# Slate DSL architecture

The Slate DSL represents a static widget tree, its properties, and explicit slots as data, then
emits ordinary Slate construction code. Dynamic state, control flow, iteration, and lifecycle stay
in handwritten host code; the generator is intentionally not a general UI runtime.

Modules may emit trees for a host class or reusable library trees. Includes and macros are resolved
at generation time, and commands provide named construction entry points. Validation keeps slot
ownership, widget argument spelling, and generated includes deterministic before source is emitted.

See [README.md](README.md) for the vertical-slice entry point.
