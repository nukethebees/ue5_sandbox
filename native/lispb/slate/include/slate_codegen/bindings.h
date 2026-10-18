#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/value_id.h>

#include <vector>
namespace slate_codegen::detail {

struct Bindings {
    std::vector<lispb::IdentifierId> names;
    std::vector<ValueId> initializers;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
