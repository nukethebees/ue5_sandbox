#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace slate_codegen::detail {

struct ManifestEntry {
    std::filesystem::path input;
};

struct Manifest {
    std::vector<ManifestEntry> entries;
    std::vector<std::filesystem::path> include_directories;
};

auto read_file(std::filesystem::path const& path) -> std::string;
auto load_manifest(std::filesystem::path const& path) -> Manifest;

}
