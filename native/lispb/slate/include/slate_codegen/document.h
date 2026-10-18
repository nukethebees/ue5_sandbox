#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/arguments.h>
#include <slate_codegen/bindings.h>
#include <slate_codegen/box_slots.h>
#include <slate_codegen/boxes.h>
#include <slate_codegen/called_widgets.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/declarations.h>
#include <slate_codegen/existing_widgets.h>
#include <slate_codegen/functions.h>
#include <slate_codegen/localized_values.h>
#include <slate_codegen/margins.h>
#include <slate_codegen/named_slots.h>
#include <slate_codegen/parameters.h>
#include <slate_codegen/value_id.h>
#include <slate_codegen/values.h>
#include <slate_codegen/widgets.h>

#include <vector>
namespace slate_codegen::detail {

struct Document {
    Widgets widgets;
    Boxes boxes;
    ExistingWidgets existing;
    CalledWidgets calls;
    Values values;
    LocalizedValues localized;
    Margins margins;
    Arguments arguments;
    NamedSlots named_slots;
    BoxSlots box_slots;
    Bindings bindings;
    Parameters parameters;
    Functions functions;
    Declarations declarations;
    std::vector<ValueId> value_lists;
    std::vector<lispb::StringId> margin_values;

    [[nodiscard]] auto child_span(ChildId const child) const -> lispb::codegen::sexpr::SourceSpan;
    [[nodiscard]] auto value_span(ValueId const value) const -> lispb::codegen::sexpr::SourceSpan;
};

}
