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
Constraints check supplied values during decoding and skip disengaged optionals.
Omitted fields retain trusted C++ defaults or the caller's existing values. Sign, string and
vector constraints reuse the existing constrained types' `try_make` functions. Prefer a
constrained member type when the invariant must survive later C++ mutation; do not repeat
the same invariant as a schema annotation.
Cross-field/domain validation remains ordinary C++. Integral bounds retain their exact
64-bit values. Do not use arbitrary C++ expressions as bounds.

For an invariant that must survive C++ mutation, a record may specify
`:constraint-policy project::policy`, naming an existing `ConstrainedValuePolicy` supplied
through the module's includes. Lispb emits the fields as `RecordValue` and exposes `Record`
as `ConstrainedValue<RecordValue, project::policy>`. Its generated value reader composes
with the ordinary constrained reader, which calls `try_make` before returning `Record`.
Defaults must satisfy the policy for the constrained type to be default-constructible.

A constrained record is replaced as one complete value, including inside generated override
records. Mark all required components with `:required true`; an override must supply those
components too. Enable `:generate-overrides` on the containing record, not on the constrained
record. This preserves cross-field invariants without exposing writable component references.
`AttackDistanceBand` uses this for its ordered minimum/desired/maximum triple. A later layer
cannot repair an invalid earlier triple.

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

Combining `:generate-overrides true` with `:generate-s7-reader true` also generates a reader
for `RecordOverrides`. Use it when a patch must be decoded before its destination exists.
The same record members supply field names, types and constraints; `:settings-exclude` members
are omitted from the patch. `:s7-name` takes precedence over `:settings-name`, otherwise names
use hyphens. Patch readers do not require fields marked required on the complete record.
Nested patches merge across ordered layers. An optional-valued member keeps two distinct states:
an absent override and a present override explicitly clearing that member with `:none`.
Apply the resulting patch using the existing generated `apply_overrides`.

`read_settings<T>(reader, node, section, existing)` in `ioj/s7/settings_reader.h` selects one
settings section and applies ordered `settings-layers`, including nested layers. It checks
structural/type and field-constraint errors during decoding. A later layer cannot hide an
invalid earlier field. Cross-field checks belong to the consumer after all layers are applied.
Other top-level sections belong to their own consumers. Generated `decode` functions are
protocol hooks; ordinary callers use `read`, `read_override` or `read_settings` so partially
decoded values cannot escape through a successful return. There is no second validation walk.

Level, entity, objective, mission, team-result, catalog/import, camera, collision-grid and
campaign records use this path. `level_inputs.lispb` describes the script-facing records.
Decode the complete input first; only then assign entity IDs, resolve objective-local
expression names, assemble domain definitions and run semantic validation. Processing
functions receive typed inputs and never access the AST. Named expressions, expression
references, outcome dependencies and unlock criteria use generated positional readers.
Recursive mission composition and leaf-payload selection still use a typed adapter.

Catalog entries capture `ReadResult<T>` so a bad imported definition remains an error
associated with that file without discarding successful siblings. Sparse settings use a
typed presentation patch and, for simulation, the remaining flattened assignments.
Presentation defaults and fields live in native records; Unreal retains resource references
and colour controls. Starfield decodes directly into its native record. Neither presentation
nor starfield uses a separate `settings-schema` mapping.
No writers are generated.
