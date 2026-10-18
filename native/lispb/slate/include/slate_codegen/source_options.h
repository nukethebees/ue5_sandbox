#pragma once

#include <filesystem>
#include <vector>

namespace slate_codegen {

struct SourceOptions {
    std::filesystem::path source_root;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> include_directories;
};

}
