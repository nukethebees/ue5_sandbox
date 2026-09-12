#pragma once

#include <lispb/compilation.h>

#include <filesystem>
#include <vector>

namespace kernel_codegen {

enum class Profile { unreal, standard, unreal_avx2_lab, native_x86_simd_lab };

struct SourceOptions {
    std::filesystem::path source_root;
    std::vector<std::filesystem::path> inputs;
    Profile profile{Profile::unreal};
};

auto compile_sources(SourceOptions const& options) -> lispb::Compilation;

}
