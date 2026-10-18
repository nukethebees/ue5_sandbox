#pragma once

#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/syntax.h>
#include <slate_codegen/document.h>

#include <filesystem>
#include <string>
#include <vector>

namespace slate_codegen::detail {

struct PreprocessedSource {
    lispb::codegen::sexpr::HandleRange forms;
    std::vector<std::filesystem::path> dependencies;
};

auto preprocess(lispb::codegen::sexpr::Syntax& syntax,
                std::filesystem::path const& input,
                std::vector<std::filesystem::path> const& include_directories)
    -> PreprocessedSource;

auto format_expansion(lispb::codegen::sexpr::Syntax const& syntax,
                      lispb::codegen::sexpr::HandleRange forms) -> std::string;

}
