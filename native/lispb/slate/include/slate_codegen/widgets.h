#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/range.h>

#include <vector>
namespace slate_codegen::detail {

struct Widgets {
    std::vector<lispb::StringId> types;
    std::vector<lispb::StringId> assigned_members;
    std::vector<Range> arguments;
    std::vector<ChildId> contents;
    std::vector<Range> named_slots;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
