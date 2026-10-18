#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <vector>
namespace slate_codegen::detail {

struct Values {
    std::vector<lispb::StringId> text;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
