#pragma once

#include <codegen/ast/node_id.h>

#include <span>
#include <string>
#include <string_view>

namespace lispb::codegen {
class CppIR;
}

namespace lispb::codegen::detail {
struct StringEnumEntry {
    std::string key;
    std::string value;
};

// Require indexed entries to map to consecutive enum values starting at zero.
auto lower_string_enum_parser(CppIR& ir,
                              std::string_view function_name,
                              std::string_view enum_type,
                              std::string_view parameter_name,
                              std::span<StringEnumEntry const> entries,
                              bool is_static = false,
                              bool indexed = false) -> NodeId;
}
