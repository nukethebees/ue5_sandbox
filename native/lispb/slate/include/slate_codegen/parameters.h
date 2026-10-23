#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/parameter_kind.h>

#include <vector>
namespace slate_codegen::detail {

struct Parameters {
    std::vector<ParameterKind> kinds;
    std::vector<lispb::StringId> names;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
};

}
