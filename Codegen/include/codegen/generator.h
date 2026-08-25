#pragma once

#include <codegen/ast.h>
#include <codegen/generated_file.h>
#include <codegen/schema.h>

#include <filesystem>
#include <vector>

namespace codegen {

auto lower_modules(Manifest const& manifest) -> std::vector<Module>;
auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile>;
auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int;

} // namespace codegen
