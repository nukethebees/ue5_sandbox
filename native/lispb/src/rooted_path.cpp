#include "lispb/path_base.h"
#include <filesystem>
#include <lispb/rooted_path.h>

namespace lispb {
auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path {
    return ((path.base == PathBase::project ? project_root : build_root) / path.path)
        .lexically_normal();
}

} // namespace lispb
