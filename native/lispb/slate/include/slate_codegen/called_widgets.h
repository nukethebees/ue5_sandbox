#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/range.h>

#include <vector>
namespace slate_codegen::detail {

struct CalledWidgets {
    std::vector<lispb::IdentifierId> parameters;
    std::vector<Range> arguments;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
