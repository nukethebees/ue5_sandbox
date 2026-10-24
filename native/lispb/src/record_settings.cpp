#include "record_settings.h"

#include <codegen/schema/declarations.h>
#include <lispb/schema/type_graph.h>

namespace lispb::codegen::detail {
auto settings_record(schema::TypeGraph const& graph,
                     ModuleHandle const module,
                     TypeRefId const reference) -> std::optional<schema::TypeId> {
    auto const& occurrences{graph.occurrences()};
    auto const row{occurrences.index(graph.resolved(module, reference))};
    auto const type{occurrences.type[row]};
    auto const declaration{graph.source_declaration(type)};
    if (occurrences.names_semantic_type[row] && !occurrences.cv_qualified[row] &&
        occurrences.physical_form[row] == PhysicalTypeForm::value && declaration.value != 0 &&
        declaration.kind() == DeclarationKind::record &&
        !graph.declarations()
             .records.constraint_policy[graph.declarations().records.index(declaration)]) {
        return type;
    }
    return std::nullopt;
}

}
