#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <vector>
namespace slate_codegen::detail {

struct LocalizedValues {
    std::vector<lispb::StringId> contexts;
    std::vector<lispb::StringId> keys;
    std::vector<lispb::StringId> text;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
