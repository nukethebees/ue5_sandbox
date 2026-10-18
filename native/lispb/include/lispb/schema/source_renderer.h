#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/module_handle.h>
#include <codegen/sexpr/indices.h>

#include <span>
#include <string>

namespace lispb {
struct CompilerStorage;
namespace codegen {
class Declarations;
}
namespace schema {
class SourceComments;

[[nodiscard]] auto render_declaration_source(codegen::Declarations const& declarations,
                                             codegen::DeclarationHandle declaration) -> std::string;
[[nodiscard]] auto render_module_source(codegen::Declarations const& declarations,
                                        codegen::ModuleHandle module) -> std::string;
// Modules must be in authored file order. A zero file handle denotes a new LF file.
[[nodiscard]] auto render_source_file(CompilerStorage const& storage,
                                      SourceComments const& comments,
                                      std::span<codegen::ModuleHandle const> modules,
                                      codegen::sexpr::SourceFileId file = {}) -> std::string;
}
}
