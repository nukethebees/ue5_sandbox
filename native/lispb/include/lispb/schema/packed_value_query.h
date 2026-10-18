#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/field_query.h>

#include <vector>

namespace lispb::schema {

class PackedValueQuery {
  public:
    PackedValueQuery(codegen::Declarations const& store,
                     std::optional<codegen::DeclarationHandle> declaration);
    auto exists() const -> bool;
    auto storage_type() const -> std::optional<std::string_view>;
    auto field(std::string_view name) const -> FieldQuery;
    auto fields() const -> std::vector<FieldQuery>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::DeclarationHandle> declaration_;
};

}
