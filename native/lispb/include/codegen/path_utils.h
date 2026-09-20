#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace codegen {

inline auto output_path_key(std::filesystem::path const& path) -> std::string {
    auto result{path.lexically_normal().generic_string()};
#ifdef _WIN32
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return result;
}

}
