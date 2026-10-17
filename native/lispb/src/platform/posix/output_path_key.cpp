#include <codegen/path_utils.h>

namespace codegen {
auto output_path_key(std::filesystem::path const& path) -> std::string {
    return path.lexically_normal().generic_string();
}
}
