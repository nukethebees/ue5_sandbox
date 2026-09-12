#pragma once

#include <lispb/compilation.h>

#include <filesystem>
#include <string>
#include <vector>

namespace slate_codegen {

struct SourceOptions {
    std::filesystem::path source_root;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> include_directories;
};

auto compile_sources(SourceOptions const& options) -> lispb::Compilation;
auto expand_sources(SourceOptions const& options) -> std::string;

}
