#include <slate_codegen/document.h>

#include <codegen/sexpr/source_span.h>
#include <slate_codegen/boxes.h>
#include <slate_codegen/called_widgets.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/child_kind.h>
#include <slate_codegen/existing_widgets.h>
#include <slate_codegen/localized_values.h>
#include <slate_codegen/margins.h>
#include <slate_codegen/value_id.h>
#include <slate_codegen/value_kind.h>
#include <slate_codegen/values.h>
#include <slate_codegen/widgets.h>

#include <cassert>
#include <vector>

namespace slate_codegen::detail {

auto Document::child_span(ChildId const child) const -> lispb::codegen::sexpr::SourceSpan {
    auto const index{child.index()};
    switch (child.kind()) {
        case ChildKind::widget:
            return widgets.spans[index];
        case ChildKind::box:
            return boxes.spans[index];
        case ChildKind::existing:
            return existing.spans[index];
        case ChildKind::call:
            return calls.spans[index];
        case ChildKind::none:
            break;
    }
    assert(false);
    return {};
}
auto Document::value_span(ValueId const value) const -> lispb::codegen::sexpr::SourceSpan {
    if (value.kind() == ValueKind::localized_text) {
        return localized.spans[value.index()];
    }
    if (value.kind() == ValueKind::margin) {
        return margins.spans[value.index()];
    }
    return values.spans[value.index()];
}

}
