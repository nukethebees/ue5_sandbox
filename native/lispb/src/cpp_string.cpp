#include <string_view>
#include <string>
#include <codegen/cpp_string.h>

namespace lispb::codegen {

void append_escaped_cpp_string(std::string& result, std::string_view const value) {
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
}

auto escape_cpp_string(std::string_view const value) -> std::string {
    std::string result;
    result.reserve(value.size());
    append_escaped_cpp_string(result, value);
    return result;
}

}
