#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace slate_codegen::detail {

struct ManifestEntry {
    std::filesystem::path input;
    std::filesystem::path output;
};

auto read_file(std::filesystem::path const& path) -> std::string;
auto load_manifest(std::filesystem::path const& path) -> std::vector<ManifestEntry>;

}
