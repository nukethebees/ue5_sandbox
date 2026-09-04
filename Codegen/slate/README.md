# Slate DSL vertical slice

This directory contains a deliberately narrow experiment that generates ordinary Slate C++ from
a static widget-tree description. It does not parse expressions, own state, or participate in the
normal Unreal build freshness check.

## Grammar

```text
document     := widget EOF
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
value        := number | boolean | quoted_text | atom
type_name    := atom
```

`;` introduces a line comment. Atoms remain opaque, allowing types such as `SSpinBox<int32>` and
constants such as `HAlign_Left`. Quoted text generates `FText::FromString(TEXT(...))`; it is only
for this non-production sample. Calls, operators, member access, bindings, events, assignment, raw
C++, localization, and existing widgets are intentionally unsupported.

Slate DSL source uses two-space indentation to keep deeply nested trees compact. Generated C++
continues to use the project's ordinary four-space indentation.

Keyword arguments must precede children. Each widget may have one direct default child and uniquely
named `(slot Name child)` forms. Box slot options are keywords and must precede their one child.

## Commands

```text
cmake --build --preset codegen --target generate-slate-code
cmake --build --preset codegen --target check-generated-slate-code
```

The compiler interface is:

```text
slatec --manifest <path> [--output-root <directory>] [--check]
```

Manifest paths are relative to the manifest. Output defaults to a `generated` directory beside the
manifest, keeping its generated-file inventory isolated from the production schema generator.

## Pilot baseline

The production `Radar3DShowcase` remains unchanged. Its current `RebuildWidget` layout expression
occupies 40 physical lines (lines 22 through 61) and contains an experiment panel with Controls and
Preview slots, two auto-height control rows, a benchmark output assignment, and a local radar
preview widget. The sample approximates only its static structure for generated-code review.
