#include <lispb/schema/document_query.h>
namespace lispb::schema {
DocumentQuery::DocumentQuery(codegen::Manifest const& manifest)
    : manifest_{&manifest} {}
auto DocumentQuery::module(std::string_view name) const -> ModuleQuery {
    return {*manifest_, name};
}
auto DocumentQuery::modules() const -> std::vector<ModuleQuery> {
    std::vector<ModuleQuery> result;
    result.reserve(manifest_->modules.size());
    for (auto const module : manifest_->modules) {
        result.emplace_back(*manifest_, module);
    }
    return result;
}
}
