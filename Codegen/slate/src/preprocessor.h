#pragma once

#include "syntax.h"

#include <filesystem>
#include <vector>

namespace slate_codegen::detail {

auto preprocess(std::filesystem::path const& input,
                std::vector<std::filesystem::path> const& include_directories)
    -> std::vector<Token>;

}
