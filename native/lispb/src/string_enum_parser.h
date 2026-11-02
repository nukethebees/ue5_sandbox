#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/node_id.h>

#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen::detail {
struct LoweringContext;

struct StringEnumEntry {
    std::string key;
    Expr value;
};

// Require indexed entries to map to consecutive enum values starting at zero.
auto lower_string_enum_parser(LoweringContext& ctx,
                              std::string_view function_name,
                              std::string_view enum_type,
                              std::string_view parameter_name,
                              std::span<StringEnumEntry const> entries,
                              bool indexed = false) -> NodeId;
}
