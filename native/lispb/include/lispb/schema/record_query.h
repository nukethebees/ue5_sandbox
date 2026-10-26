#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/child_query.h>

#include <vector>

namespace lispb::schema {

class RecordQuery {
  public:
    RecordQuery(codegen::Declarations const& store,
                std::optional<codegen::DeclarationHandle> declaration);
    auto exists() const -> bool;
    auto storage_type() const -> std::optional<std::string_view>;
    auto member(std::string_view name) const -> ChildQuery;
    auto members() const -> std::vector<ChildQuery>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::DeclarationHandle> declaration_;
};

}
