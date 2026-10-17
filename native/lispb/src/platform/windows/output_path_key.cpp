#include <codegen/path_utils.h>

#include <algorithm>
#include <cctype>

namespace codegen {
auto output_path_key(std::filesystem::path const& path) -> std::string {
    auto result{path.lexically_normal().generic_string()};
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}
}
