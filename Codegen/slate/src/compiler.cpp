#include <slate_codegen/compiler.h>

#include "preprocessor.h"
#include "manifest.h"
#include "parser.h"
#include "renderer.h"

#include <codegen/generated_file.h>
#include <codegen/generator.h>

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
    std::vector<codegen::GeneratedFile> files;
    std::set<std::string> owners;
    for (auto const& entry : manifest.entries) {
        auto tokens{detail::preprocess(manifest_directory / entry.input, manifest.include_directories)};
        auto const document{detail::parse(entry.input.generic_string(), std::move(tokens))};
        for (auto const& widget_declaration : document.declarations) {
            if (!owners.insert(widget_declaration.name).second) {
                throw detail::SourceError{
                    entry.input.generic_string(),
                    widget_declaration.span,
                    "duplicate widget declaration '" + widget_declaration.name + "'"};
            }
            files.push_back(codegen::GeneratedFile{
                output_path(widget_declaration.name),
                detail::render(entry.input.generic_string(), widget_declaration)});
        }
    }
    return codegen::generate_files(files, output_root, output_root, options.check);
}

}
