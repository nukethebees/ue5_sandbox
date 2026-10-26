#include <lispb/schema/integer_domain.h>

#include <codegen/schema/declarations.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/type_graph.h>

#include <optional>

namespace lispb::schema {

auto integer_domain(TypeGraph const& graph, TypeId const type) -> std::optional<TypeDefinitionId> {
    auto const definition{graph.definition(type)};
    auto const& definitions{graph.definitions()};
    if (auto const scalar{((definition).kind() == lispb::schema::TypeDefinitionKind::integer_scalar
                               ? std::optional{definition}
                               : std::nullopt)}) {
        return scalar;
    }
    if (auto const external{((definition).kind() == lispb::schema::TypeDefinitionKind::external
                                 ? std::optional{definition}
                                 : std::nullopt)};
        external &&
        definitions.externals.integer_semantics[definitions.externals.index(*external)]) {
        return *definitions.externals.integer_semantics[definitions.externals.index(*external)];
    }
    return std::nullopt;
}

}
