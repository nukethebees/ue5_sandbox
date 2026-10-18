#include <lispb/schema/source_renderer.h>

#include "canonical_renderer.h"

#include <lispb/compiler_storage.h>
#include <lispb/schema/source_comments.h>

#include <utility>

namespace lispb::schema {

auto render_declaration_source(codegen::Declarations const& declarations,
                               codegen::DeclarationHandle const declaration) -> std::string {
    return detail::CanonicalRenderer{declarations}.render_declaration_schema(declaration);
}

auto render_module_source(codegen::Declarations const& declarations,
                          codegen::ModuleHandle const module) -> std::string {
    return detail::CanonicalRenderer{declarations}.render_module(module);
}

auto render_source_file(CompilerStorage const& storage,
                        SourceComments const& comments,
                        std::span<codegen::ModuleHandle const> const modules,
                        codegen::sexpr::SourceFileId const file) -> std::string {
    std::string rendered;
    for (auto const module : modules) {
        if (!rendered.empty()) {
            rendered += '\n';
        }
        rendered += render_module_source(storage.declarations, module);
        rendered += '\n';
    }
    return comments.apply(storage, modules, file, std::move(rendered));
}

} // namespace lispb::schema
