#pragma once

#include <string>
#include <string_view>

namespace codegen {

inline auto append_cpp_escaped_character(std::string& output, char const character) -> bool {
    switch (character) {
        case '\\':
            output += "\\\\";
            return true;
        case '"':
            output += "\\\"";
            return true;
        case '\n':
            output += "\\n";
            return true;
        case '\r':
            output += "\\r";
            return true;
        case '\t':
            output += "\\t";
            return true;
        default:
            return false;
    }
}

inline auto escape_cpp_string(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (auto const character : value) {
        if (!append_cpp_escaped_character(result, character)) {
            result += character;
        }
    }
    return result;
}

}
