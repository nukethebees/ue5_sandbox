#pragma once

#include <codegen/sexpr/handle_range.h>
#include <lispb/indices.h>
#include <lispb/path_base.h>

#include <vector>
namespace lispb {

struct TargetColumns {
    std::vector<StringId> names;
    std::vector<codegen::sexpr::HandleRange> sources;
    std::vector<StringId> output_paths;
    std::vector<PathBase> output_bases;
};

}
