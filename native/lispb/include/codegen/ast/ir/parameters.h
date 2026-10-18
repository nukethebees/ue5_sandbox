#pragma once

#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Parameters {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<std::optional<StringId>> default_value;
};

} // namespace lispb::codegen::ir
