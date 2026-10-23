#pragma once

#include <lispb/indices.h>
#include <slate_codegen/value_id.h>

#include <vector>
namespace slate_codegen::detail {

struct Arguments {
    std::vector<lispb::StringId> names;
    std::vector<ValueId> values;
};

}
