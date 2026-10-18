#include <slate_codegen/compiler.h>

#include "parser.h"
#include "preprocessor.h"
#include "renderer.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compilation.h>
#include <lispb/strings.h>
#include <slate_codegen/document.h>
#include <slate_codegen/indices.h>
#include <slate_codegen/source_options.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace slate_codegen {

void compile_sources(lispb::codegen::sexpr::Syntax& syntax,
                     detail::Document& document,
                     lispb::codegen::CppIR& ir,
                     lispb::Compilation& result,
                     SourceOptions const& options,
                     bool const emit) {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    std::vector<std::filesystem::path> include_directories;
    include_directories.reserve(options.include_directories.size());
    for (auto const& directory : options.include_directories) {
        include_directories.push_back(
            (directory.is_absolute() ? directory : source_root / directory).lexically_normal());
    }

    for (auto const& input : options.inputs) {
        auto const preprocessed{
            detail::preprocess(syntax, source_root / input, include_directories)};
        result.dependencies.insert(result.dependencies.end(),
                                   preprocessed.dependencies.begin(),
                                   preprocessed.dependencies.end());
        auto const begin{document.declarations.size()};
        detail::parse(syntax, preprocessed.forms, document);
        auto const end{document.declarations.size()};
        for (auto index{begin}; index < end; ++index) {
            if (emit) {
                auto const file{
                    detail::lower(input.generic_string(),
                                  syntax.sources(),
                                  document,
                                  detail::DeclarationId{static_cast<std::uint32_t>(index)},
                                  ir)};
                auto const file_index{ir.files.index(file)};
                result.add_text(std::filesystem::path{ir.strings.view(ir.files.path[file_index])},
                                ir.render(file),
                                ir.files.flags[file_index].format_generated());
            }
        }
    }
}

auto expand_sources(lispb::codegen::sexpr::Syntax& syntax, SourceOptions const& options)
    -> std::string {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    std::vector<std::filesystem::path> include_directories;
    include_directories.reserve(options.include_directories.size());
    for (auto const& directory : options.include_directories) {
        include_directories.push_back(
            (directory.is_absolute() ? directory : source_root / directory).lexically_normal());
    }

    std::string result;
    for (auto const& input : options.inputs) {
        auto const preprocessed{
            detail::preprocess(syntax, source_root / input, include_directories)};
        if (!result.empty()) {
            result += '\n';
        }
        result += detail::format_expansion(syntax, preprocessed.forms);
    }
    return result;
}

}
