#pragma once

#include <codegen/generated_file.h>

#include <filesystem>
#include <vector>

namespace lispb {

auto publish_generated_files(std::vector<codegen::GeneratedFile> const& files,
                             std::filesystem::path const& project_root,
                             std::filesystem::path const& output_root,
                             bool check_only) -> int;

} // namespace lispb
