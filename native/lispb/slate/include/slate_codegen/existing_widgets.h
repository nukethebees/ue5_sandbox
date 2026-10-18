#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>

#include <vector>
namespace slate_codegen::detail {

struct ExistingWidgets {
    std::vector<lispb::IdentifierId> parameters;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
