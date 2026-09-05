#include <kernel_codegen/compiler.h>

#include "manifest.h"
#include "parser.h"
#include "renderer.h"

#include <codegen/generator.h>
#include <codegen/sexpr/lexer.h>

#include <filesystem>
#include <iterator>
#include <utility>
#include <vector>

namespace kernel_codegen {

auto compile_manifest(CompileOptions const& options) -> int {
    auto const manifest_path{std::filesystem::absolute(options.manifest).lexically_normal()};
    auto const manifest_directory{manifest_path.parent_path()};
    auto const output_root{options.output_root
                               ? std::filesystem::absolute(*options.output_root).lexically_normal()
                               : manifest_directory / "generated"};
    auto const manifest{detail::load_manifest(manifest_path)};
    auto const profile{options.profile == Profile::unreal ? detail::Profile::unreal
                                                          : detail::Profile::standard};
    std::vector<codegen::GeneratedFile> files;
    for (auto const& entry : manifest.entries) {
        auto const input_path{manifest_directory / entry.input};
        auto const source{detail::read_file(input_path)};
        auto const document{detail::parse(entry.input.generic_string(),
                                          codegen::sexpr::lex(entry.input.generic_string(), source))};
        for (auto const& module : document.modules) {
            auto rendered{detail::render(module, profile)};
            files.insert(files.end(),
                         std::make_move_iterator(rendered.begin()),
                         std::make_move_iterator(rendered.end()));
        }
    }
    return codegen::generate_files(files, output_root, output_root, options.check);
}

}
