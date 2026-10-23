#pragma once

#include <codegen/sexpr/indices.h>
#include <lispb/indices.h>
#include <slate_codegen/range.h>

#include <vector>

namespace slate_codegen::detail {

struct Macros {
    std::vector<Range> parameters;
    std::vector<lispb::codegen::sexpr::FormId> bodies;
    std::vector<lispb::StringId> parameter_names;
};

}
