#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace kernel_codegen {

enum class Profile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };

struct CompileOptions {
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> output_root;
    Profile profile{Profile::unreal};
    bool check{false};
};

struct SourceOptions {
    std::filesystem::path source_root;
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output_root;
    Profile profile{Profile::unreal};
    bool check{false};
};

auto compile_manifest(CompileOptions const& options) -> int;
auto compile_sources(SourceOptions const& options) -> int;

}
