# Slate DSL vertical slice

This directory contains a deliberately narrow experiment that generates ordinary Slate C++ from
a static widget-tree description. It does not parse expressions, own state, or participate in the
normal Unreal build freshness check.

## Grammar

```text
document     := widget_class+ EOF
widget_class := "(" "widget-class" qualified_name function+ ")"
function     := "(" "function" identifier child ")"
widget       := "(" type_name widget_item* ")"
widget_item  := argument | named_slot | child
argument     := keyword value
named_slot   := "(" "slot" atom child ")"
child        := widget | vbox
vbox         := "(" "vbox" box_slot+ ")"
box_slot     := "(" ("auto" | "fill") box_option* child ")"
box_option   := ":weight" number
              | ":padding" margin
              | ":halign" ("left" | "center" | "right" | "fill")
              | ":valign" ("top" | "center" | "bottom" | "fill")
margin       := number
              | "(" number number ")"
              | "(" number number number number ")"
value        := number | boolean | quoted_text | atom | callable
callable     := "(" "callback" identifier ")"
              | "(" "method" identifier ")"
              | "(" "uobject" identifier ")"
type_name    := atom
qualified_name := identifier ("::" identifier)*
```

`;` introduces a line comment. Atoms remain opaque, allowing types such as `SSpinBox<int32>` and
constants such as `HAlign_Left`. Quoted text generates `FText::FromString(TEXT(...))`; it is only
for this non-production sample. Calls, operators, member access, assignment, raw C++, localization,
and existing widgets are intentionally unsupported.

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

The generated header is included by the handwritten implementation after the owner is complete. A
`callback` value becomes a forwarding-reference function parameter and an `_Lambda` attribute. It
may be consumed once. `method` binds the friend-held owner through the ordinary Slate attribute
overload, while `uobject` selects the `_UObject` lifetime-aware overload. A generated function can
then be called normally:

```cpp
return SlateGenerated::URadar3DShowcaseBuilder{*this}.RebuildWidget(on_value_changed);
```

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

## Pilot baseline

The production `Radar3DShowcase` remains unchanged. Its current `RebuildWidget` layout expression
occupies 40 physical lines (lines 22 through 61) and contains an experiment panel with Controls and
Preview slots, two auto-height control rows, a benchmark output assignment, and a local radar
preview widget. The sample approximates only its static structure for generated-code review.
