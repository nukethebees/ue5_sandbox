#include <slate_codegen/compiler.h>

#include "parser.h"
#include "preprocessor.h"
#include "renderer.h"

#include <filesystem>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace slate_codegen {
namespace {

auto output_path(std::string_view owner) -> std::filesystem::path {
    std::filesystem::path result;
    while (true) {
        auto const separator{owner.find("::")};
        auto const component{owner.substr(0, separator)};
        if (separator == std::string_view::npos) {
            result /= std::string{component} + ".slate.generated.h";
            return result;
        }
        result /= component;
        owner.remove_prefix(separator + 2);
    }
}

}

auto compile_sources(SourceOptions const& options) -> lispb::Compilation {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    std::vector<std::filesystem::path> include_directories;
    include_directories.reserve(options.include_directories.size());
    for (auto const& directory : options.include_directories) {
        include_directories.push_back(
            (directory.is_absolute() ? directory : source_root / directory).lexically_normal());
    }

    lispb::Compilation result;
    std::set<std::string> owners;
    for (auto const& input : options.inputs) {
        auto preprocessed{detail::preprocess(source_root / input, include_directories)};
        result.dependencies.insert(result.dependencies.end(),
                                   preprocessed.dependencies.begin(),
                                   preprocessed.dependencies.end());
        auto const document{detail::parse(input.generic_string(), std::move(preprocessed.forms))};
        for (auto const& widget_declaration : document.declarations) {
            if (!owners.insert(widget_declaration.name).second) {
                throw detail::SourceError{input.generic_string(),
                                          widget_declaration.span,
                                          "duplicate widget declaration '" +
                                              widget_declaration.name + "'"};
            }
            result.artifacts.push_back(
                lispb::TextArtifact{output_path(widget_declaration.name),
                                    detail::render(input.generic_string(), widget_declaration)});
        }
    }
    return result;
}

auto expand_sources(SourceOptions const& options) -> std::string {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    std::vector<std::filesystem::path> include_directories;
    include_directories.reserve(options.include_directories.size());
    for (auto const& directory : options.include_directories) {
        include_directories.push_back(
            (directory.is_absolute() ? directory : source_root / directory).lexically_normal());
    }

    std::string result;
    for (auto const& input : options.inputs) {
        auto const preprocessed{detail::preprocess(source_root / input, include_directories)};
        if (!result.empty()) {
            result += '\n';
        }
        result += detail::format_expansion(preprocessed.forms);
    }
    return result;
}

}
