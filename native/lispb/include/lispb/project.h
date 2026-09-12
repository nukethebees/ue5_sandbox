#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace lispb {

enum class PathBase { project, build };

struct RootedPath {
    PathBase base{PathBase::project};
    std::filesystem::path path;
};

struct CppSchemaTarget {
    std::string name;
    std::filesystem::path types;
    std::vector<std::filesystem::path> sources;
    RootedPath output_root;
};

struct SlateTarget {
    std::string name;
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> include_directories;
    RootedPath output_root;
};

enum class KernelProfile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };

struct KernelTarget {
    std::string name;
    std::vector<std::filesystem::path> sources;
    KernelProfile profile{KernelProfile::unreal};
    RootedPath output_root;
};

struct MaterialTarget {
    std::string name;
    std::filesystem::path source;
    RootedPath artifact;
};

using Target = std::variant<CppSchemaTarget, SlateTarget, KernelTarget, MaterialTarget>;

struct Project {
    std::filesystem::path root;
    std::map<std::string, Target, std::less<>> targets;
    std::map<std::string, std::vector<std::string>, std::less<>> groups;
};

auto load_project(std::filesystem::path const& path) -> Project;
auto resolve(RootedPath const& path,
             std::filesystem::path const& project_root,
             std::filesystem::path const& build_root) -> std::filesystem::path;
auto expand_target(Project const& project, std::string const& name) -> std::vector<std::string>;

} // namespace lispb
