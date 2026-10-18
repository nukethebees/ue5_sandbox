#pragma once

#include <codegen/schema/manifest.h>
#include <lispb/schema/declaration_query.h>
#include <lispb/schema/packed_value_query.h>
#include <lispb/schema/soa_query.h>

namespace lispb::schema {

class ModuleQuery {
  public:
    ModuleQuery(codegen::Manifest const& manifest, std::string_view name);
    ModuleQuery(codegen::Manifest const& manifest, codegen::ModuleHandle module);
    auto exists() const -> bool;
    auto name() const -> std::string_view;
    auto declarations() const -> std::vector<DeclarationQuery>;
    auto declaration(std::string_view name) const -> DeclarationQuery;
    auto packed_value(std::string_view name) const -> PackedValueQuery;
    auto soa(std::string_view name) const -> SoaQuery;
  private:
    auto find(std::string_view name, codegen::DeclarationKind kind) const
        -> std::optional<codegen::DeclarationHandle>;
    codegen::Manifest const* manifest_;
    std::optional<codegen::ModuleHandle> module_;
};

}
