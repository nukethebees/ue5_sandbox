#include "single_allocation_soa_model.h"

#include "lowering_context.h"

#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace lispb::codegen::detail {
auto schema_namespace(LoweringContext& ctx, DeclarationHandle schema) -> std::string {
    auto const scoped{ctx.for_declaration(schema)};
    auto const& decls{ctx.declarations};
    auto const& settings{decls.module_settings_store};
    auto const namespace_name{settings.namespace_name[decls.module_settings(scoped.module)]};
    return (namespace_name ? ctx.text(*namespace_name) + "::" : "") +
           soa_namespace_name(ctx.text(decls.soas.name[schema]));
}

auto build_single_allocation_model(LoweringContext& ctx, DeclarationHandle schema)
    -> SingleAllocationModel {
    SingleAllocationModel model{schema, {}};
    std::set<std::string> names;
    auto collect = [&](auto&& self, DeclarationHandle current, std::string const& prefix) -> void {
        auto const local{ctx.for_declaration(current)};
        auto const& members{ctx.declarations.soa_members};
        for (auto member : ctx.declarations.items(ctx.declarations.soas.members[current])) {
            auto const name{prefix + ctx.text(members.name[member])};
            if (members.kind[member] == SoaMemberKind::nested) {
                self(self, local.nested_declaration(member), name + "_");
            } else {
                if (!names.insert(name).second) {
                    throw std::invalid_argument{"Single-allocation column name collision: " + name};
                }
                model.columns.push_back({name, local.resolve(members.type[member])});
            }
        }
    };
    collect(collect, schema, "");
    return model;
}

} // namespace lispb::codegen::detail
