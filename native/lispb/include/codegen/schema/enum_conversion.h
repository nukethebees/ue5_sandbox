#pragma once

#include <array>
#include <string_view>

namespace codegen {

enum class EnumConversion {
    lex_to_string,
    string_view,
    string,
    lex_to_display_string,
    display_string_view,
    display_string,
    lex_to_serialized_string,
    try_parse_serialized,
};

inline constexpr auto all_enum_conversions() -> std::array<EnumConversion, 8> {
    return {
        EnumConversion::lex_to_string,
        EnumConversion::string_view,
        EnumConversion::string,
        EnumConversion::lex_to_display_string,
        EnumConversion::display_string_view,
        EnumConversion::display_string,
        EnumConversion::lex_to_serialized_string,
        EnumConversion::try_parse_serialized,
    };
}

inline constexpr auto enum_conversion_name(EnumConversion const conversion) -> std::string_view {
    switch (conversion) {
        case EnumConversion::lex_to_string:
            return "lex-to-string";
        case EnumConversion::string_view:
            return "string-view";
        case EnumConversion::string:
            return "string";
        case EnumConversion::lex_to_display_string:
            return "lex-to-display-string";
        case EnumConversion::display_string_view:
            return "display-string-view";
        case EnumConversion::display_string:
            return "display-string";
        case EnumConversion::lex_to_serialized_string:
            return "lex-to-serialized-string";
        case EnumConversion::try_parse_serialized:
            return "try-parse-serialized";
    }
    return "unknown";
}

} // namespace codegen
