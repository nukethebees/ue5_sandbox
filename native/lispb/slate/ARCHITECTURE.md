# Slate DSL architecture

The Slate DSL represents a static widget tree, its properties, and explicit slots as data, then
emits ordinary Slate construction code. Dynamic state, control flow, iteration, and lifecycle stay
in handwritten host code; the generator is intentionally not a general UI runtime.

Modules may emit trees for a host class or reusable library trees. Includes and macros are resolved
at generation time, and commands provide named construction entry points. Validation keeps slot
ownership, widget argument spelling, and generated includes deterministic before source is emitted.

The central compiler owns the shared strings, source files, syntax, C++ IR, and Slate document.
Parsing consumes shared form views directly. Macro substitution retains form handles and shared
provenance instead of copying trees or flattening them back into tokens.

The document stores widget, box, existing-widget, and factory-call payloads in separate columns.
Four-byte tagged child and value handles connect those columns; ordered arguments, slots,
parameters, and bindings occupy flat ranges. Names refer to interned identifiers and source text
uses string handles. Validation resolves parameter roles before lowering.

Common C++ IR owns generated declarations, signatures, namespaces, includes, and file rendering.
Slate-specific builder expressions lower to raw body nodes because their bracket/slot syntax is a
separate domain. Validation stops before this lowering when no output is requested.
