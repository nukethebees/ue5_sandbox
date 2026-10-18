#pragma once

#include <lispb/path_base.h>

#include <filesystem>
namespace lispb {

struct RootedPath {
    PathBase base{PathBase::project};
    std::filesystem::path path;
};

auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path;

}
