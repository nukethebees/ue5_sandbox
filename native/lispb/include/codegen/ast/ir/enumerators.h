#pragma once

#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Enumerators {
    std::vector<StringId> name;
    std::vector<std::optional<StringId>> initializer;
    std::vector<std::optional<StringId>> annotation;
};

} // namespace lispb::codegen::ir
