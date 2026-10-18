#include "soa_body.h"

#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace lispb::codegen::detail {

void validate_soa_body_columns(Declarations const& declarations,
                               DeclarationHandle schema,
                               std::map<IdentifierId, DeclarationHandle> const& schemas,
                               std::string_view const body) {
    scan_soa_columns(
        body,
        declarations.strings().view(declarations.soas.name[declarations.soas.index(schema)]),
        [&](std::size_t, std::size_t, std::span<std::string const> const path) {
            auto const* current{&schema};
            auto const count{path.size()};
            for (std::size_t index{}; index < count; ++index) {
                auto const members{declarations.items(
                    declarations.soas.members[declarations.soas.index(*current)])};
                auto const found{std::ranges::find(members, path[index], [&](auto id) {
                    return declarations.strings().view(
                        declarations.soa_members.name[declarations.soa_members.index(id)]);
                })};
                if (found == members.end()) {
                    throw std::invalid_argument{
                        "SOA '" +
                        std::string{declarations.strings().view(
                            declarations.soas.name[declarations.soas.index(*current)])} +
                        "' has no column '" + path[index] + "'"};
                }
                auto const member{*found};
                if (index + 1 < count) {
                    if (declarations.soa_members.kind[declarations.soa_members.index(member)] !=
                            SoaMemberKind::nested ||
                        !declarations.soa_members
                             .nested_schema[declarations.soa_members.index(member)]) {
                        throw std::invalid_argument{
                            "SOA column path requires a resolved nested schema: " + path[index]};
                    }
                    current =
                        &schemas.at(*declarations.soa_members
                                         .nested_schema[declarations.soa_members.index(member)]);
                }
            }
        });
}

}
