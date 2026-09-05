#pragma once

#include <filesystem>
#include <optional>

namespace kernel_codegen {

enum class Profile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };

struct CompileOptions {
    std::filesystem::path manifest;
    std::optional<std::filesystem::path> output_root;
    Profile profile{Profile::unreal};
    bool check{false};
};

auto compile_manifest(CompileOptions const& options) -> int;

}
