#pragma once

#include <lispb/output.h>
#include <lispb/project.h>

#include <filesystem>
#include <string>

namespace lispb {

struct CompiledTarget {
    Compilation compilation;
    PublicationOptions publication;
};

auto compile_target(Target const& target,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& build_root) -> CompiledTarget;
auto expand_target_source(Target const& target, std::filesystem::path const& project_root)
    -> std::string;

} // namespace lispb
