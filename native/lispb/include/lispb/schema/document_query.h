#pragma once
#include <lispb/schema/module_query.h>

namespace lispb::schema {
// Query views borrow the document and expire when its state changes.
class DocumentQuery {
  public:
    DocumentQuery(codegen::Declarations const& declarations,
                  std::span<codegen::ModuleHandle const> modules);
    auto module(std::string_view name) const -> ModuleQuery;
    auto modules() const -> std::vector<ModuleQuery>;
  private:
    codegen::Declarations const* declarations_;
    std::span<codegen::ModuleHandle const> modules_;
};
}
