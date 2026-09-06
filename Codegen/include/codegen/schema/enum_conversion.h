#pragma once

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

} // namespace codegen
