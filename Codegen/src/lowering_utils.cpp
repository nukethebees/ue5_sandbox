#include "lowering_utils.h"

#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <algorithm>
#include <cctype>
#endif

namespace codegen::detail {

auto join(std::vector<std::string> const& values, std::string_view separator) -> std::string {
    std::ostringstream output;
    auto const count{values.size()};
    for (std::size_t index{0}; index < count; ++index) {
        if (index != 0) {
            output << separator;
        }
        output << values[index];
    }
    return output.str();
}

auto join_lines(std::vector<std::string> const& values) -> std::string {
    return join(values, "\n");
}

auto source_include(ModuleSettings const& settings) -> std::string {
    if (settings.header_include.has_value()) {
        return *settings.header_include;
    }
    return settings.header.filename().generic_string();
}

auto dependency_for_key(std::string const& key,
                        std::map<std::string, CppType> const& types) -> TypeDependency {
    auto const found{types.find(key)};
    if (found == types.end() || found->second.dependencies.empty()) {
        throw std::invalid_argument{"Unknown dependency type: " + key};
    }
    return found->second.dependencies.front();
}

auto qualify(CppType type, std::string const& suffix) -> CppType {
    type.spelling += suffix;
    return type;
}

auto output_path_key(std::filesystem::path const& path) -> std::string {
    auto result{path.lexically_normal().generic_string()};
#ifdef _WIN32
    std::ranges::transform(result, result.begin(), [](unsigned char const character) {
        return static_cast<char>(std::tolower(character));
    });
#endif
    return result;
}

} // namespace codegen::detail
