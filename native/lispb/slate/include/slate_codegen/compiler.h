#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace slate_codegen {

struct CompileOptions {
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> output_root;
    bool check{false};
};

struct SourceOptions {
    std::filesystem::path source_root;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> include_directories;
    std::filesystem::path output_root;
    bool check{false};
};

auto compile_manifest(CompileOptions const& options) -> int;
auto expand_manifest(std::filesystem::path const& manifest) -> std::string;
auto compile_sources(SourceOptions const& options) -> int;
auto expand_sources(SourceOptions const& options) -> std::string;

}
