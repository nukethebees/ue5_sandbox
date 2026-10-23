#pragma once
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct Enums {
    std::vector<StringId> name;
    std::vector<CppTypeId> underlying_type;
    std::vector<Range> values;
    std::vector<StringId> export_specifier;
    std::vector<std::uint8_t> scoped;
};

} // namespace lispb::codegen::ir
