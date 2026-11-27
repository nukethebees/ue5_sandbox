#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/child_occurrences.h>
#include <lispb/indices.h>

#include <span>
#include <vector>

namespace lispb::codegen::detail {
struct LoweringContext;

struct EnumAccessorMember {
    EnumAccessorKey key;
    StringId field;
    CppTypeId value_type;
    std::vector<StringId> elements{};
};

auto lower_enum_accessors(LoweringContext& context,
                          std::span<EnumAccessorMember const> members,
                          bool packed) -> Nodes;
}
