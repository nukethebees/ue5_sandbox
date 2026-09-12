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
`facade-module`, `settings-module`, and `umbrella-module`. Declaration order is significant because
it determines generated-file order. `;` introduces a line comment.

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
      :body ("ml::subtract_in_place(remaining_times, dt);")
      (parameter dt (type-ref float :suffix " const")))))
```

A type reference is normally an atom or quoted C++ spelling. Use `(type-ref <name> :suffix <text>
:nested <name>)` when a reference needs additional structure. Arbitrary C++ lines, paths, labels,
and other text containing whitespace or semicolons must be quoted. Strings support `\\`, `\"`,
`\n`, `\r`, and `\t` escapes.

Shared types are top-level declarations in `types.lispb`:

```lisp
(type registry_handle
  :spelling "FRegistryEntityHandle"
  :header "SandboxNative/RegistryEntityHandle.h"
  :pass-by value
  (operation add-element add :pass-by value)
  (operation remove-at-swap remove_at_swap))
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
