#pragma once

#include <lispb/compilation.h>

#include <filesystem>

namespace lispb {

[[nodiscard]] auto compile_material(std::filesystem::path const& input,
                                    std::filesystem::path const& project_root,
                                    std::filesystem::path output) -> Compilation;

} // namespace lispb
