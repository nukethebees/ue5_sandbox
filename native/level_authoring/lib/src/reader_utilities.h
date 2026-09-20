#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ml::level_authoring::reader_detail {
inline auto indexed_path(std::string_view const path, std::int64_t const index) -> std::string {
    return std::string{path} + "[" + std::to_string(index) + "]";
}

inline void lowercase_ascii(std::string& value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
}
}
