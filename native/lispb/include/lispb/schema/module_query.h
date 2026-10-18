#pragma once

#include <codegen/schema/module_handle.h>
#include <lispb/schema/declaration_query.h>
#include <lispb/schema/packed_value_query.h>
#include <lispb/schema/soa_query.h>

#include <span>

namespace lispb::schema {

class ModuleQuery {
  public:
    ModuleQuery(codegen::Declarations const& declarations,
                std::span<codegen::ModuleHandle const> modules,
                std::string_view name);
    ModuleQuery(codegen::Declarations const& declarations, codegen::ModuleHandle module);
    auto exists() const -> bool;
    auto name() const -> std::string_view;
    auto declarations() const -> std::vector<DeclarationQuery>;
    auto declaration(std::string_view name) const -> DeclarationQuery;
    auto packed_value(std::string_view name) const -> PackedValueQuery;
    auto soa(std::string_view name) const -> SoaQuery;
  private:
    auto find(std::string_view name, codegen::DeclarationKind kind) const
        -> std::optional<codegen::DeclarationHandle>;
    codegen::Declarations const* declarations_;
    std::optional<codegen::ModuleHandle> module_;
};

}
