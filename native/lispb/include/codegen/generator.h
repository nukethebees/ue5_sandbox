#pragma once

#include <codegen/ast.h>
#include <codegen/generated_file.h>
#include <codegen/schema.h>
#include <lispb/output.h>

#include <filesystem>
#include <span>
#include <vector>

namespace codegen {

auto lower_modules(Manifest const& manifest) -> std::vector<Module>;
auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile>;
auto compile_sources(std::filesystem::path const& types,
                     std::span<std::filesystem::path const> modules) -> lispb::Compilation;
inline auto generate_files(std::vector<GeneratedFile> const& files,
                           std::filesystem::path const& project_root,
                           std::filesystem::path const& output_root,
                           bool check_only) -> int {
    lispb::Compilation compilation;
    for (auto const& file : files) {
        compilation.artifacts.push_back(file);
    }
    return lispb::publish(
        compilation,
        {.path_base = project_root, .output_root = output_root, .check_only = check_only});
}

} // namespace codegen
