#include <ioj/layout/analyzer/physical_type_spelling.hpp>

#include "../enum_backing.hpp"

#include <codegen/ast/cpp_types.h>
#include <codegen/schema.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/external_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace ioj::layout {
using namespace analysis_detail;

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<std::string> {
    auto const& definitions{types.definitions()};
    auto const& occurrences{types.occurrences()};
    auto const& cpp_types{types.cpp_types()};
    std::vector<lispb::schema::TypeId> visited;
    auto current{type};
    while (lispb::schema::is_valid(current) &&
           std::ranges::find(visited, current) == visited.end()) {
        visited.push_back(current);
        auto const definition{types.definition(current)};
        if (definition.kind() == lispb::schema::TypeDefinitionKind::external) {
            auto const& externals{definitions.externals};
            return lispb::codegen::native_spelling(
                std::string{cpp_types.spelling(externals.cpp_type[externals.index(definition)])});
        }
        if (definition.kind() == lispb::schema::TypeDefinitionKind::enumeration) {
            auto const underlying_type{definitions.enums.underlying_type[definition]};
            if (lispb::schema::is_valid(underlying_type)) {
                if (!occurrences.contains_value(underlying_type)) {
                    return std::string{cpp_types.spelling(occurrences.cpp_type[underlying_type])};
                }
                current = occurrences.type[underlying_type];
                continue;
            }
            return derived_enum_backing_type(types, definition);
        }
        if (lispb::schema::is_packed_projection(types, current)) {
            return "std::uint" + std::to_string(types.bit_layout(current).storage_bits) + "_t";
        }
        return std::nullopt;
    }
    return std::nullopt;
}

auto physical_type_spelling(lispb::schema::TypeGraph const& types,
                            lispb::schema::ResolvedOccurrenceId const use)
    -> std::optional<std::string> {
    auto const& occurrences{types.occurrences()};
    return occurrences.contains_value(use)
             ? physical_type_spelling(types, occurrences.type[use])
             : std::optional{std::string{types.cpp_types().spelling(occurrences.cpp_type[use])}};
}

} // namespace ioj::layout
