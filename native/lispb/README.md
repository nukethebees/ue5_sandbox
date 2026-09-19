# Lispb code generation

`lispb` is the single native frontend for the C++ schema, Slate, Kernel, and Material generators.
All authored inputs use the `.lispb` extension, and `lispb/project.lispb` is the authoritative
project manifest. Named targets specify their dialect inputs and outputs; groups compose targets
for repository-wide generation. Generated text files are tracked in `.lispb-outputs`.

```lisp
(lispb-project
  :language-version 1
  :project-root ".."
  (cpp-schema sandbox-code
    :types "lispb/schema/types.lispb"
    :sources ("lispb/schema/core_layouts.lispb")
    :output-root (project-path ".")))
```

Module files contain one or more top-level declarations. Supported declarations are `soa-module`,
`vector-soa-module`, `homogeneous-soa-module`, `enum-module`, `static-table-module`,
`packed-value-module`, `facade-module`, `settings-module`, and `umbrella-module`. Declaration order
is significant because it determines generated-file order. `;` introduces a line comment.

Packed values are header-only wrappers around one unsigned integer. Fields occupy consecutive bits
from least significant to most significant in declaration order; enum fields are marked explicitly
because general C++ type references do not otherwise carry enum metadata:

```lisp
(packed-value-module fighter_values
  :header "sandbox/simulation/fighter_state.h"
  :namespace ml::simulation
  (packed-value FighterState
    :storage std::uint32_t
    (field entity_index std::uint32_t :bits 24)
    (field state @fighter_state :bits 8 :kind enum)))
```

Generated packed values provide raw construction/access, typed getters, checked and fallible
setters, and raw-value three-way comparison. Supported storage and integer field types are the
unsigned 8-, 16-, 32-, and 64-bit Unreal or standard-library types. `bool` fields use exactly one
bit; signed fields, explicit offsets, and reserved interior ranges are not currently supported.

Module names and domain identifiers are positional. Optional metadata uses kebab-case keyword
properties, and owned declarations are nested forms:

```lisp
(soa-module sandbox_core_countdown_timers
  :header "Plugins/SandboxCore/Source/SandboxCore/Public/SandboxCore/countdown_timers.h"
  :source "Plugins/SandboxCore/Source/SandboxCore/Private/countdown_timers.cpp"
  :header-include "SandboxCore/countdown_timers.h"
  :include-order ("SpaceGame/" "Sandbox/" "SandboxCore/")

  (struct FCountdownTimers
    :operations (all)
    :export-specifier SANDBOXCORE_API
    (member remaining_times array float)
    (function tick void
      :noexcept true
      :definition-in-source true
      :dependencies (array_math)
      :body #cpp{ml::subtract_in_place(remaining_times, dt);}cpp#
      (parameter dt (type-ref float :suffix " const")))))
```

`soa-module` defaults to the Unreal backend. Native libraries can select
`:backend standard-library` and omit `:source`; the generated owning storage uses
`std::vector`, views use `std::span`, and nested and single-allocation layouts remain available.
Native targets compiling that output must include `native/lispb/native_soa/include` as well as
`native/core/include`.

```lisp
(soa-module simulation_contacts
  :header "generated/contacts.h"
  :backend standard-library
  :namespace sandbox::simulation
  (struct Contacts
    :operations (all)
    (member entity_ids array std::uint32_t)
    (member distances array float)))
```

An SOA struct can generate a compact named field mask from its array members. The generator
selects `uint8`, `uint16`, `uint32`, or `uint64` storage from the resulting field count. Optional
dimensions reserve a contiguous row-major range for an indexed field:

```lisp
(struct FHistory
  :field-enum-name EHistoryField
  :field-mask-name FHistoryFieldMask
  (member validity_masks array FHistoryFieldMask)
  (member active_entities array uint32 :mask-field true)
  (member active_entities_by_type array uint32
    :mask-field true
    :mask-dimensions ((entity_type_index "ml::EnumCountTrait<EEntityType>::count_value"))))
```

A type reference is normally an atom or quoted C++ spelling. Use `(type-ref <name> :suffix <text>
:nested <name>)` when a reference needs additional structure. Paths, labels, scalar C++ fragments,
and other text containing whitespace or semicolons must be quoted. Strings support `\\`, `\"`,
`\n`, `\r`, and `\t` escapes.

Function `:body`, module `:prelude`, and facade `:validation` may instead use an opaque C++ raw
literal:

```lisp
:body #cpp{if (dt <= 0.0f) {
    return;
}

ml::subtract_in_place(remaining_times, dt);}cpp#
```

Everything between `#cpp{` and the first exact `}cpp#` is preserved as text. LispB does not
interpret escapes, comments, strings, parentheses, or balanced braces inside it. Leading and
trailing newlines and indentation are part of the value; place the first and last C++ characters
next to the delimiters when those framing newlines are not wanted. The exact closing sequence
cannot occur in the embedded C++; use the existing quoted-list syntax in that rare case. Quoted
forms remain supported.

Shared types are top-level declarations in `types.lispb`:

```lisp
(type native_unique_id
  :spelling "EntityUniqueId"
  :header "ioj/sim/entity_unique_id.h"
  :pass-by value)
```

Unknown and duplicate properties or declarations are errors. Syntax and schema diagnostics include
the source file, line, and column. Semantic validation after parsing continues to use the shared
Codegen schema validation.

The native command selects an action and named target:

```text
lispb <generate|check|validate|expand|dump-ir> --target <name>
  [--project <file>] [--build-root <directory>] [--depfile <file>]
```

Generate or check committed files from the repository root:

```text
cmake --workflow --preset generate-code
cmake --build --preset codegen --target check-generated-code
```

The generated C++ test fixture is committed under `tests/compile_fixture/generated` so it can be
reviewed and analysed directly. Regeneration compares file contents and leaves unchanged files
untouched:

```text
cmake --build --preset debug-game --target generate-codegen-compile-fixture
cmake --build --preset debug-game --target check-generated-codegen-compile-fixture
```

The DebugGame and Development worktree-setup workflows regenerate committed C++ and Slate outputs
before checking them. When this changes files, `csetup` prints a warning and records the affected
paths in its workflow log so they can be reviewed and committed.
