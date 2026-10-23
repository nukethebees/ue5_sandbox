#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/range.h>

#include <vector>
namespace slate_codegen::detail {

struct Functions {
    std::vector<lispb::StringId> names;
    std::vector<Range> parameters;
    std::vector<Range> bindings;
    std::vector<ChildId> roots;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
