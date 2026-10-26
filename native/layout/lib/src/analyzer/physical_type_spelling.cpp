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
    std::vector<lispb::schema::TypeId> visited;
    auto current{type};
    while (lispb::schema::is_valid(current) &&
           std::ranges::find(visited, current) == visited.end()) {
        visited.push_back(current);
        auto const& node{current};
        if (auto const external{
                ((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::external
                     ? std::optional{types.definition(node)}
                     : std::nullopt)}) {
            return lispb::codegen::native_spelling(std::string{types.cpp_types().spelling(
                types.definitions()
                    .externals.cpp_type[types.definitions().externals.index(*external)])});
        }
        if (auto const enumeration{
                ((types.definition(node)).kind() == lispb::schema::TypeDefinitionKind::enumeration
                     ? std::optional{types.definition(node)}
                     : std::nullopt)}) {
            if (lispb::schema::is_valid(
                    types.definitions()
                        .enums.underlying_type[types.definitions().enums.index(*enumeration)])) {
                if (!types.occurrences().contains_value(
                        types.definitions().enums.underlying_type[types.definitions().enums.index(
                            *enumeration)])) {
                    return std::string{types.cpp_types().spelling(
                        types.occurrences().cpp_type[types.occurrences().index(
                            types.definitions().enums.underlying_type
                                [types.definitions().enums.index(*enumeration)])])};
                }
                current = types.occurrences().type[types.occurrences().index(
                    types.definitions()
                        .enums.underlying_type[types.definitions().enums.index(*enumeration)])];
                continue;
            }
            return derived_enum_backing_type(types, *enumeration);
        }
        if (lispb::schema::is_packed_projection(types, node)) {
            return "std::uint" + std::to_string(types.bit_layout(node).storage_bits) + "_t";
        }
        return std::nullopt;
    }
    return std::nullopt;
}

auto physical_type_spelling(lispb::schema::TypeGraph const& types,
                            lispb::schema::ResolvedOccurrenceId const use)
    -> std::optional<std::string> {
    return types.occurrences().contains_value(use)
             ? physical_type_spelling(types,
                                      types.occurrences().type[types.occurrences().index(use)])
             : std::optional{std::string{types.cpp_types().spelling(
                   types.occurrences().cpp_type[types.occurrences().index(use)])}};
}

} // namespace ioj::layout
