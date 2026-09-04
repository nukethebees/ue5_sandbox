# Slate DSL vertical slice

This directory contains a deliberately narrow experiment that generates ordinary Slate C++ from
a static widget-tree description. It does not parse expressions or own state. The editor target
checks that committed generated Slate headers are current before invoking UnrealBuildTool.

## Grammar

```text
document     := (include | macro | widget_class | widget_library)+ EOF
include      := "(" "include" quoted_path ")"
macro        := "(" "defmacro" name "(" name* ")" expression ")"
widget_class := "(" "widget-class" qualified_name function+ ")"
widget_library := "(" "widget-library" qualified_name function+ ")"
function     := "(" "function" identifier params (child | let) ")"
params       := "(" "params" parameter* ")"
parameter    := "(" ("value" | "callback" | "factory" | "existing") identifier ")"
let          := "(" "let" "(" binding+ ")" child ")"
binding      := "(" identifier (value | margin) ")"
widget       := "(" type_name widget_item* ")"
assigned     := "(" "assign" identifier type_name widget_item* ")"
existing     := "(" "existing" identifier ")"
call         := "(" "call" identifier value* ")"
widget_item  := argument | named_slot | child
argument     := keyword value
named_slot   := "(" "slot" atom child ")"
child        := widget | assigned | existing | call | box
box          := vbox | hbox
vbox         := "(" "vbox" box_slot+ ")"
hbox         := "(" "hbox" box_slot+ ")"
box_slot     := "(" ("auto" | "fill") box_option* child ")"
box_option   := ":weight" (number | value_parameter)
              | ":padding" margin
              | ":halign" ("left" | "center" | "right" | "fill" | value_parameter)
              | ":valign" ("top" | "center" | "bottom" | "fill" | value_parameter)
margin       := number
              | identifier
              | "(" number number ")"
              | "(" number number number number ")"
value        := number | boolean | quoted_text | localized_text | atom | callable
localized_text := "(" "loc" quoted_text quoted_text quoted_text ")"
callable     := "(" "callback" identifier ")"
              | "(" "method" identifier ")"
              | "(" "uobject" identifier ")"
type_name    := atom
qualified_name := identifier ("::" identifier)*
```

`;` introduces a line comment. Atoms remain opaque, allowing types such as `SSpinBox<int32>` and
constants such as `HAlign_Left`. Quoted text generates `FText::FromString(TEXT(...))`. A `loc`
form generates `NSLOCTEXT(context, key, text)`. Calls, operators, member access, and raw C++ remain
intentionally unsupported.

`let` is restricted to one optional wrapper directly inside a function. It emits sequential
`auto const` locals, so later bindings may refer to earlier ones. A margin tuple emits an `FMargin`
local and can be referenced from `:padding`; a scalar binding used as padding is converted to an
`FMargin`. Bindings cannot be nested, shadowed, or initialized with callable forms.

Slate DSL source uses two-space indentation to keep deeply nested trees compact. Generated C++
continues to use the project's ordinary four-space indentation.

Keyword arguments must precede children. Each widget may have one direct default child and uniquely
named `(slot Name child)` forms. Box slot options are keywords and must precede their one child.

A document may declare multiple owning widget classes. Each `widget-class` produces an independent
generated header containing one friend builder struct, and each `function` becomes a builder member
function. Generated output paths follow the qualified owner name: `Example::SPanel` produces
`Example/SPanel.slate.generated.h`. A handwritten owner forward-declares and friends its builder:

```cpp
namespace SlateGenerated {
struct URadar3DShowcaseBuilder;
}

class URadar3DShowcase {
    friend struct SlateGenerated::URadar3DShowcaseBuilder;
};
```

The generated header is included by the handwritten implementation after the owner is complete.
Every generated function explicitly declares its handwritten boundary in `params`; declaration
order determines the generated C++ signature. Every parameter must be used with its declared role.
A `value` is an ordinary symbol used by widget arguments, factory arguments, let initializers, or
padding and may be reused. A `callback` becomes a forwarding-reference function parameter and an `_Lambda` attribute.
`existing` similarly forwards a widget supplied by handwritten code into the generated tree, while
`assign` generates `SAssignNew` against a member of the friend-held owner. Callback and existing
parameters may each be consumed once. `call` invokes a supplied `factory` parameter with DSL values
and may reuse that factory within the same function; callable forms are not accepted as its
arguments. `method` binds the owner through the ordinary Slate attribute overload, while `uobject`
selects the `_UObject` lifetime-aware overload. A generated function can then be called normally:

```cpp
return SlateGenerated::URadar3DShowcaseBuilder{*this}.RebuildWidget(
    on_value_changed, radar_widget);
```

## Libraries without a host class

Use `widget-library` for construction functions that take everything they need as parameters:

```lisp
(widget-library HeatmapBenchmark
  (function BuildHeatmap
    (params (value style))
    (SHeatmap2D :Style style)))
```

This produces `HeatmapBenchmark.slate.generated.h` with inline functions in
`SlateGenerated::HeatmapBenchmark`. Include it after the required Slate widget headers, then call:

```cpp
auto widget{SlateGenerated::HeatmapBenchmark::BuildHeatmap(style)};
```

No handwritten host type, friend declaration, or builder instance is needed. Qualified library
names work too: `Example::Controls` produces `Example/Controls.slate.generated.h` and namespace
`SlateGenerated::Example::Controls`. Parameterless functions are also inline so headers can be
included in multiple translation units.

Libraries support values, callbacks, existing widgets, factories, `let`, and macros. `assign`,
`method`, and `uobject` require a host and are rejected in libraries, including after macro
expansion. Keep `widget-class` for those forms. Classes and libraries can share a source file or
manifest, but their declaration names must be unique because those names determine output paths.

## Macros and includes

Macros expand before widget parsing and validation. Each macro takes a fixed number of positional
arguments and returns one expression. `$parameter` substitutes an entire atom or list; strings are
never interpolated. A macro may expand to a function, slot, widget, or another supported form.

```lisp
(defmacro padded (padding child)
  (vbox
    (auto :padding $padding $child)))

(widget-class SPanel
  (function Build
    (params (existing content))
    (padded (0 0 0 10) (existing content))))
```

Macro names start with a lowercase letter and contain letters, digits, underscores, or hyphens.
They cannot replace built-in forms. Definitions are collected before expansion, so declaration
order does not matter. Macros may call other macros, but direct and indirect recursive expansion
are rejected. There is no evaluation, variadic splicing, or automatic renaming: literal names in
the template refer to the caller's scope. Pass member and binding names explicitly when sharing
templates. Normal parameter-use rules still apply after expansion, including the single-use rule
for callbacks and existing widgets.

Shared libraries contain only `defmacro` and `include` declarations:

```lisp
(include "Experiments/Patterns.sbxslate")

(widget-class SRadar3DWidget
  (image-builder image_))
```

Configure search roots in the manifest:

```json
{
  "include_directories": ["../Widgets"],
  "entries": [{"input": "Radar3DShowcase.sbxslate"}]
}
```

Relative include directories are resolved against the manifest directory; absolute directories are
also accepted. Include paths themselves must be relative. Lookup checks beside the including file
first, then configured roots in order, taking the first matching file. Missing-file errors list the
searched locations. Includes are loaded once per input document using canonical file paths; cycles
and duplicate macro definitions are errors. Macro definitions do not leak between manifest inputs.
Include and macro declarations must appear at the top level and cannot be generated by macros.

Diagnostics retain the macro definition and invocation locations. Generated `#line` directives
use paths relative to the input document's directory, including for macro library code. The CMake
generate/check targets always invoke the compiler and re-read all transitive includes, so editing
a shared macro is picked up on the next generation or freshness check without reconfiguring CMake.

## Commands

```text
cmake --build --preset codegen --target generate-slate-code
cmake --build --preset codegen --target check-generated-slate-code
```

The compiler interface is:

```text
slatec --manifest <path> [--output-root <directory>] [--check]
```

Manifest input paths are relative to the manifest. Output defaults to a `generated` directory beside
the manifest, keeping its generated-file inventory isolated from the production schema generator.
Output filenames are derived from the widget class declarations rather than listed in the manifest.

## Pilot

The production pilots keep stateful widgets, callbacks, and dynamically produced child widgets in
handwritten `RebuildWidget` functions, then pass them into generated builders. Static layout,
localized text, output assignment, and owner callback binding are generated.
