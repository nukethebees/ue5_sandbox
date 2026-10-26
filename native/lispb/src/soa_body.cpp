#include "soa_body.h"

#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lispb::codegen::detail {

void validate_soa_body_columns(schema::TypeGraph const& graph,
                               ModuleHandle module,
                               DeclarationHandle schema,
                               std::string_view const body) {
    auto const& decls{graph.declarations()};
    scan_soa_columns(
        body,
        decls.strings().view(decls.soas.name[schema]),
        [&](std::size_t, std::size_t, std::span<std::string const> const path) {
            auto current{schema};
            auto current_module{module};
            auto const count{path.size()};
            for (std::size_t index{}; index < count; ++index) {
                auto const members{decls.items(decls.soas.members[current])};
                auto const found{std::ranges::find(members, path[index], [&](auto id) {
                    return decls.strings().view(decls.soa_members.name[id]);
                })};
                if (found == members.end()) {
                    throw std::invalid_argument{
                        "SOA '" + std::string{decls.strings().view(decls.soas.name[current])} +
                        "' has no column '" + path[index] + "'"};
                }
                auto const member{*found};
                if (index + 1 < count) {
                    if (decls.soa_members.kind[member] != SoaMemberKind::nested) {
                        throw std::invalid_argument{
                            "SOA column path requires a resolved nested schema: " + path[index]};
                    }
                    auto const occurrence{
                        graph.resolved(current_module, decls.soa_members.type[member])};
                    auto const& occurrences{graph.occurrences()};
                    auto const nested{occurrences.type[occurrences.index(occurrence)]};
                    current = graph.source_declaration(nested);
                    current_module = graph.module(nested);
                }
            }
        });
}

}
