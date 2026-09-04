#include <slate_codegen/compiler.h>

#include "lexer.h"
#include "manifest.h"
#include "parser.h"
#include "renderer.h"

#include <codegen/generated_file.h>
#include <codegen/generator.h>

#include <filesystem>
#include <utility>
#include <vector>

namespace slate_codegen {
namespace {

auto render_entry(std::filesystem::path const& manifest_directory,
                  detail::ManifestEntry const& entry) -> codegen::GeneratedFile {
    auto const source{detail::read_file(manifest_directory / entry.input)};
    auto tokens{detail::lex(entry.input.generic_string(), source)};
    auto const root{detail::parse(entry.input.generic_string(), std::move(tokens))};
    return codegen::GeneratedFile{entry.output,
                                  detail::render(entry.input.generic_string(), root)};
}

}

auto compile_manifest(CompileOptions const& options) -> int {
    auto const manifest_path{std::filesystem::absolute(options.manifest).lexically_normal()};
    auto const manifest_directory{manifest_path.parent_path()};
    auto const output_root{options.output_root
                               ? std::filesystem::absolute(*options.output_root).lexically_normal()
                               : manifest_directory / "generated"};
    auto const entries{detail::load_manifest(manifest_path)};
    std::vector<codegen::GeneratedFile> files;
    files.reserve(entries.size());
    for (auto const& entry : entries) {
        files.push_back(render_entry(manifest_directory, entry));
    }
    return codegen::generate_files(files, output_root, output_root, options.check);
}

}
