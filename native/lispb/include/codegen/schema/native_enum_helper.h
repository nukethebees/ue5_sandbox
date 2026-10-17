#pragma once

#include <string_view>

namespace codegen {

enum class NativeEnumHelper {
    to_string,
    try_parse,
    to_display_string,
    to_serialized_string,
    try_parse_serialized,
    traits,
};

constexpr auto native_enum_helper_name(NativeEnumHelper const helper) -> std::string_view {
    switch (helper) {
        case NativeEnumHelper::to_string:
            return "to-string";
        case NativeEnumHelper::try_parse:
            return "try-parse";
        case NativeEnumHelper::to_display_string:
            return "to-display-string";
        case NativeEnumHelper::to_serialized_string:
            return "to-serialized-string";
        case NativeEnumHelper::try_parse_serialized:
            return "try-parse-serialized";
        case NativeEnumHelper::traits:
            return "traits";
    }
    return {};
}

} // namespace codegen
