#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace slate_codegen {

struct CompileOptions {
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> output_root;
    bool check{false};
};

auto compile_manifest(CompileOptions const& options) -> int;
auto expand_manifest(std::filesystem::path const& manifest) -> std::string;

}
