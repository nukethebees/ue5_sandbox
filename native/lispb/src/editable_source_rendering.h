#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/module_handle.h>
#include <lispb/schema/source_range.h>

#include <optional>
#include <string>

namespace lispb {
struct CompilerStorage;
}

namespace lispb::schema::detail {

auto render_declaration_source(CompilerStorage& storage,
                               codegen::DeclarationHandle declaration,
                               std::optional<SourceRange> original) -> std::string;
auto render_editable_module(CompilerStorage& storage, codegen::ModuleHandle module)
    -> std::optional<std::string>;

} // namespace lispb::schema::detail
