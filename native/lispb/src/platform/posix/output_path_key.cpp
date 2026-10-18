#include <codegen/path_utils.h>

#include <filesystem>
#include <string>

namespace lispb::codegen {
auto output_path_key(std::filesystem::path const& path) -> std::string {
    return path.lexically_normal().generic_string();
}
}
