#pragma once

#include <string>
#include <string_view>

namespace codegen {

inline auto escape_cpp_string(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    for (auto const character : value) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default: {
                auto const byte{static_cast<unsigned char>(character)};
                if (byte < 32 || byte == 127) {
                    result += '\\';
                    result += static_cast<char>('0' + ((byte >> 6) & 7));
                    result += static_cast<char>('0' + ((byte >> 3) & 7));
                    result += static_cast<char>('0' + (byte & 7));
                } else {
                    result += character;
                }
                break;
            }
        }
    }
    return result;
}

}
