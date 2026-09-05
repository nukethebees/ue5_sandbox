#pragma once

#include <filesystem>
#include <optional>

namespace kernel_codegen {

struct CompileOptions {
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> output_root;
    bool check{false};
};

auto compile_manifest(CompileOptions const& options) -> int;

}
