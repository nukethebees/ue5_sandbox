#include <slate_codegen/compiler.h>

#include "manifest.h"
#include "parser.h"
#include "preprocessor.h"
#include "renderer.h"

#include <codegen/generated_file.h>
#include <lispb/output.h>

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

auto compile_manifest(CompileOptions const& options) -> int {
    auto const manifest_path{std::filesystem::absolute(options.manifest).lexically_normal()};
    auto const manifest_directory{manifest_path.parent_path()};
    auto const output_root{options.output_root
                               ? std::filesystem::absolute(*options.output_root).lexically_normal()
                               : manifest_directory / "generated"};
    auto const manifest{detail::load_manifest(manifest_path)};
    std::vector<std::filesystem::path> inputs;
    inputs.reserve(manifest.entries.size());
    for (auto const& entry : manifest.entries) {
        inputs.push_back(entry.input);
    }
    return compile_sources(SourceOptions{.source_root = manifest_directory,
                                         .inputs = std::move(inputs),
                                         .include_directories = manifest.include_directories,
                                         .output_root = output_root,
                                         .check = options.check});
}

auto compile_sources(SourceOptions const& options) -> int {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    auto const output_root{std::filesystem::absolute(options.output_root).lexically_normal()};
    std::vector<std::filesystem::path> include_directories;
    include_directories.reserve(options.include_directories.size());
    for (auto const& directory : options.include_directories) {
        include_directories.push_back(
            (directory.is_absolute() ? directory : source_root / directory).lexically_normal());
    }

    std::vector<codegen::GeneratedFile> files;
    std::set<std::string> owners;
    for (auto const& input : options.inputs) {
        auto tokens{detail::preprocess(source_root / input, include_directories)};
        auto const document{detail::parse(input.generic_string(), std::move(tokens))};
        for (auto const& widget_declaration : document.declarations) {
            if (!owners.insert(widget_declaration.name).second) {
                throw detail::SourceError{input.generic_string(),
                                          widget_declaration.span,
                                          "duplicate widget declaration '" +
                                              widget_declaration.name + "'"};
            }
            files.push_back(
                codegen::GeneratedFile{output_path(widget_declaration.name),
                                       detail::render(input.generic_string(), widget_declaration)});
        }
    }
    return lispb::publish_generated_files(files, output_root, output_root, options.check);
}

auto expand_manifest(std::filesystem::path const& manifest) -> std::string {
    auto const path{std::filesystem::absolute(manifest).lexically_normal()};
    auto const loaded{detail::load_manifest(path)};
    std::vector<std::filesystem::path> inputs;
    inputs.reserve(loaded.entries.size());
    for (auto const& entry : loaded.entries) {
        inputs.push_back(entry.input);
    }
    return expand_sources(SourceOptions{.source_root = path.parent_path(),
                                        .inputs = std::move(inputs),
                                        .include_directories = loaded.include_directories});
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
        auto const tokens{detail::preprocess(source_root / input, include_directories)};
        if (!result.empty()) {
            result += '\n';
        }
        result += detail::format_expansion(tokens);
    }
    return result;
}

}
