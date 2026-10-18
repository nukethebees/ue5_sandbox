#pragma once
#include <lispb/schema/module_query.h>

namespace lispb::schema {
// Query views borrow the document and expire when its state changes.
class DocumentQuery {
  public:
    explicit DocumentQuery(codegen::Manifest const& manifest);
    auto module(std::string_view name) const -> ModuleQuery;
    auto modules() const -> std::vector<ModuleQuery>;
  private:
    codegen::Manifest const* manifest_;
};
}
