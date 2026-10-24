# S7 record readers

Opt ordinary records into AST decoding without changing their C++ layout or adding an
interpreter dependency to their normal header:

```lisp
(module example :header "Example.h" :namespace project
  (record Example :generate-s7-reader true :s7-tag "example"
    (member count int :initializer "2" :nonnegative true)
    (member scale float :initializer "1.f" :positive true :finite true)
    (member label @string :required true :nonempty true :s7-name "name")))
```

This emits `Example.h` and `Example_reader.h`. Include the latter and call
`ioj::s7::read<project::Example>(reader, node, "example")`. The input is an existing
owned `s7::Ast`; evaluate scripts before decoding. The result is
`std::expected<T, s7::AstDiagnostics>`, with field/index paths and diagnostic codes.

Fields use their C++ names with underscores replaced by hyphens. `:s7-name` overrides
that spelling. The record consumes a keyword-tagged list such as
`(:example :count 3 :name "demo")`. Unknown, duplicate, missing required and malformed
fields are errors. Optional *presence* preserves the C++ initializer; `:required true`
requires the field even if its C++ type has a default. Required members may have
non-default-constructible constrained types.

Add `:s7-positional true` for fixed positional records. Members consume arguments in
declaration order: `(record Pair :generate-s7-reader true :s7-tag pair :s7-positional true
(member first int) (member second int))` reads `(:pair 1 2)`. The tag and exact arity are
checked before decoding; diagnostics still use member names. Every argument must be present,
including an optional-valued argument (use `:none` to disengage it). Variadic expression
composition and tagged-alternative selection remain handwritten.

Readers default-construct a local record when C++ supports it, then decode into its members.
Otherwise they stage values for aggregate construction. Schema initializers make defaults
explicit and let the generator omit unused construction helpers. Required-field presence is
independent of value storage. Generated field enums use compact integer storage; presence uses
the existing `FieldMask` through 64 fields, with a bitset fallback above that. Required checks
compute `missing = required & ~seen` and enumerate names only when the mask is nonempty.

`:initializer` contains C++ initializer text. Single-token literals can be unquoted,
for example `:initializer 1.0f`; quote expressions containing spaces and use `""` for
empty brace initialization. Numeric member types remain ordinary C++ numeric types.
This is Lispb's schema parser, not S7 evaluation: it accepts either an atom or a quoted token
for C++ text, interns that text, and emits it inside the member's braces. Both `1.0f` and
`"1.0f"` therefore produce the same C++ initializer. Other properties have their own parsers:
booleans, counts, numeric bounds and type references are not all stored as C++ strings.

Member constraints are `:nonzero`, `:positive`, `:nonnegative`, `:finite`, `:nonempty`,
and inclusive numeric `:minimum` / `:maximum`. Boolean constraints opt in with `true`.
Constraints apply to the completed value, including defaults, and skip disengaged
optionals. Prefer existing constrained C++ types when they already express the invariant.
Cross-field/domain validation remains ordinary C++. Integral bounds retain their exact
64-bit values. Do not use arbitrary C++ expressions as bounds.

The common reader supports integers (including exact ratios/reals), floating-point
numbers, booleans, strings, native vectors, serialized enums, constrained values,
`std::vector<T>`, `std::array<T, N>`, and `std::optional<T>`. Containers recursively use
the element reader. `:none` explicitly disengages an optional; `#f` and `()` remain
boolean/list values. Numeric conversion checks range before casting. Floating-point
readers preserve nonfinite values unless a constraint forbids them.

To extend decoding, specialize `ioj::s7::TypeReader<T>` with a static
`read(Reader const&, NodeIndex, std::string_view) -> ReadResult<T>`.
`Reader::symbol` handles symbol-valued identifiers, while the string reader requires a
string. Unsupported member types fail compilation with a reader-specific diagnostic.
Direct generated-record dependencies include their companion readers automatically.
Use record `:s7-reader-includes ("CustomReader.h")` for handwritten adapters or dependencies
hidden inside externally registered container aliases. All readers in one module are
declared before their definitions, allowing recursive container composition.

`read_override(reader, node, path, existing)` takes its seed by value. Omitted members retain
their values, nested records are partially updated, and success returns the modified copy.
Errors never change an lvalue seed. Required fields need not be repeated in an override;
creating a previously absent optional record still requires its required fields. Scalars and
collections replace their entire value. `:none` explicitly clears an optional.

`read_settings<T>(reader, node, section, existing)` in `ioj/s7/settings_reader.h` selects one
settings section and applies ordered `settings-layers`, including nested layers. It checks
structural/type errors during decoding and validates the effective record after all layers.
Other top-level sections belong to their own consumers. Generated `decode` and `validate`
functions are protocol hooks; ordinary callers use `read`, `read_override` or `read_settings`
so partially decoded or unvalidated values cannot escape through a successful return.

Level, entity, objective, mission, team-result, catalog/import, camera, collision-grid and
campaign records use this path. `level_inputs.lispb` describes the script-facing records.
Decode the complete input first; only then assign entity IDs, resolve objective-local
expression names, assemble domain definitions and run semantic validation. Processing
functions receive typed inputs and never access the AST. Named expressions, expression
references, outcome dependencies and unlock criteria use generated positional readers.
Recursive mission composition and leaf-payload selection still use a typed adapter.

Catalog entries capture `ReadResult<T>` so a bad imported definition remains an error
associated with that file without discarding successful siblings. Sparse settings use a
typed adapter that retains authored presence and ordered layers. Their existing generated
field bindings apply those assignments afterward; omitted values never insert defaults.
Starfield instead decodes directly into its native record through the same generated-reader
protocol, with no flattened assignments or separate `settings-schema` mapping.
No writers are generated.
