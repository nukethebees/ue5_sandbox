# Slate DSL vertical slice

This directory contains a deliberately narrow experiment that generates ordinary Slate C++ from
a static widget-tree description. It does not parse expressions, own state, or participate in the
normal Unreal build freshness check.

## Grammar

```text
document     := widget EOF
widget       := "widget" type_name "{" widget_item* "}"
widget_item  := argument | content | named_slot
argument     := identifier "=" value
content      := "content" child_block
named_slot   := "slot" identifier child_block
child_block  := "{" child "}"
child        := widget | vbox
vbox         := "vbox" "{" box_slot+ "}"
box_slot     := ("auto" | "fill" ["weight" "=" number]) box_option* child_block
box_option   := "padding" "=" margin
              | "halign" "=" ("left" | "center" | "right" | "fill")
              | "valign" "=" ("top" | "center" | "bottom" | "fill")
margin       := number
              | "(" number "," number ")"
              | "(" number "," number "," number "," number ")"
value        := number | boolean | quoted_text | qualified_identifier
type_name    := qualified_identifier ["<" type_name ("," type_name)* ">"]
```

`//` introduces a line comment. Quoted text generates `FText::FromString(TEXT(...))`; it is only
for this non-production sample. Qualified identifiers allow constants such as `HAlign_Left`, but
calls, operators, member access, bindings, events, assignment, raw C++, localization, and existing
widgets are intentionally unsupported.

Slate DSL source uses two-space indentation to keep deeply nested trees compact. Generated C++
continues to use the project's ordinary four-space indentation.

Arguments must precede child slots. Each widget may have one `content` child and uniquely named
`slot` children. A child block contains exactly one widget or vertical box.

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
