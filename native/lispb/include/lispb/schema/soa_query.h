#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/member_query.h>

#include <vector>

namespace lispb::schema {

class SoaQuery {
  public:
    SoaQuery(codegen::Declarations const& store,
             std::optional<codegen::DeclarationHandle> declaration);
    auto exists() const -> bool;
    auto member(std::string_view name) const -> MemberQuery;
    auto members() const -> std::vector<MemberQuery>;
  private:
    codegen::Declarations const* store_;
    std::optional<codegen::DeclarationHandle> declaration_;
};

}
