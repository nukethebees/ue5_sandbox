#pragma once

#include "syntax.h"

#include <codegen/sexpr/reader.h>

#include <filesystem>
#include <vector>

namespace slate_codegen::detail {

struct PreprocessedSource {
    std::vector<codegen::sexpr::Form> forms;
    std::vector<std::filesystem::path> dependencies;
};

auto preprocess(std::filesystem::path const& input,
                std::vector<std::filesystem::path> const& include_directories)
    -> PreprocessedSource;

auto format_expansion(std::vector<codegen::sexpr::Form> const& forms) -> std::string;

}
