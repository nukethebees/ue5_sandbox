#pragma once

#include <codegen/schema.h>

#include <filesystem>
#include <string>
#include <vector>

namespace codegen {

struct GeneratedFile {
    std::filesystem::path path;
    std::string content;

    auto operator==(GeneratedFile const&) const -> bool = default;
};

auto lower_modules(Manifest const& manifest) -> std::vector<Module>;
auto render_modules(std::vector<Module> const& modules) -> std::vector<GeneratedFile>;
auto generate_files(std::vector<GeneratedFile> const& files,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& output_root,
                    bool check_only) -> int;

} // namespace codegen
