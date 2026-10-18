#include <lispb/schema/document_query.h>
namespace lispb::schema {
DocumentQuery::DocumentQuery(codegen::Declarations const& declarations,
                             std::span<codegen::ModuleHandle const> modules)
    : declarations_{&declarations}
    , modules_{modules} {}
auto DocumentQuery::module(std::string_view name) const -> ModuleQuery {
    return {*declarations_, modules_, name};
}
auto DocumentQuery::modules() const -> std::vector<ModuleQuery> {
    std::vector<ModuleQuery> result;
    result.reserve(modules_.size());
    for (auto const module : modules_) {
        result.emplace_back(*declarations_, module);
    }
    return result;
}
}
