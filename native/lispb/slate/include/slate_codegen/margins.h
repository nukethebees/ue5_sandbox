#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/range.h>

#include <vector>
namespace slate_codegen::detail {

struct Margins {
    std::vector<Range> values;
    std::vector<lispb::IdentifierId> bindings;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
