#include "fixed_soa_internal.h"
#include "lowering_context.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_member_kind.h>

#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {

auto fixed_leaf_argument(LoweringContext& context, FixedLeafId leaf) -> std::string {
    auto const& leaves{context.ir.lowering.fixed_leaves};
    return context.path_text(leaves.path[leaves.index(leaf)], "_");
}

auto build_soa_layout(LoweringContext& context,
                      DeclarationHandle schema,
                      std::map<IdentifierId, DeclarationHandle> const& schemas,
                      bool fixed,
                      LoweringPathId prefix,
                      std::set<IdentifierId> ancestors) -> FixedLayoutId {
    auto& storage{context.ir.lowering};
    auto const& soas{context.declarations.soas};
    auto const& sources{context.declarations.soa_members};
    auto const row{soas.index(schema)};
    if (fixed && !soas.fixed[row]) {
        throw std::invalid_argument{"SOA '" + context.text(soas.name[row]) +
                                    "' has no fixed configuration"};
    }
    if (!ancestors.insert(soas.name[row]).second) {
        throw std::invalid_argument{"SOA schema cycle at '" + context.text(soas.name[row]) + "'"};
    }

    auto const resolved{resolve_members(context, schema)};
    std::vector<FixedMemberLayoutId> members;
    std::vector<FixedLeafId> leaves;
    auto const source_members{context.declarations.items(soas.members[row])};
    auto const count{source_members.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const source{source_members[index]};
        auto const source_row{sources.index(source)};
        auto const path{storage.paths.append(prefix, sources.name[source_row])};
        auto const member{storage.fixed_member_layouts.append()};
        auto const member_row{storage.fixed_member_layouts.index(member)};
        storage.fixed_member_layouts.source[member_row] = source;
        storage.fixed_member_layouts.member[member_row] = resolved[index];
        if (sources.kind[source_row] == SoaMemberKind::array) {
            auto const leaf{storage.fixed_leaves.append()};
            auto const leaf_row{storage.fixed_leaves.index(leaf)};
            storage.fixed_leaves.path[leaf_row] = path;
            storage.fixed_leaves.type[leaf_row] =
                storage.resolved_members
                    .element_type[storage.resolved_members.index(resolved[index])];
            storage.fixed_member_layouts.leaves[member_row] = storage.save(std::span{&leaf, 1});
            leaves.push_back(leaf);
        } else {
            auto const reference{fixed ? sources.fixed_schema[source_row]
                                       : sources.nested_schema[source_row]};
            if (!reference) {
                throw std::invalid_argument{"SOA '" + context.text(soas.name[row]) +
                                            "' nested member '" +
                                            context.text(sources.name[source_row]) + "' has no " +
                                            (fixed ? "fixed_schema" : "nested_schema")};
            }
            auto const found{schemas.find(*reference)};
            if (found == schemas.end() || (fixed && !soas.fixed[soas.index(found->second)])) {
                throw std::invalid_argument{"Unknown nested schema: " + context.text(*reference)};
            }
            auto const nested{
                build_soa_layout(context, found->second, schemas, fixed, path, ancestors)};
            auto const nested_row{storage.fixed_layouts.index(nested)};
            auto const nested_leaves{storage.fixed_layouts.leaves[nested_row]};
            storage.fixed_member_layouts.leaves[member_row] = nested_leaves;
            auto const values{storage.items(nested_leaves)};
            leaves.insert(leaves.end(), values.begin(), values.end());
            if (fixed) {
                auto const config{*soas.fixed[soas.index(found->second)]};
                auto const& configs{context.declarations.fixed_soas};
                storage.fixed_member_layouts.nested_storage_name[member_row] =
                    configs.storage_name[configs.index(config)];
            }
        }
        members.push_back(member);
    }
    auto const result{storage.fixed_layouts.append()};
    auto const result_row{storage.fixed_layouts.index(result)};
    storage.fixed_layouts.schema[result_row] = schema;
    storage.fixed_layouts.members[result_row] =
        storage.save(std::span<FixedMemberLayoutId const>{members});
    storage.fixed_layouts.leaves[result_row] = storage.save(std::span<FixedLeafId const>{leaves});
    return result;
}

auto build_fixed_layout(LoweringContext& context,
                        DeclarationHandle schema,
                        std::map<IdentifierId, DeclarationHandle> const& schemas,
                        LoweringPathId prefix,
                        std::set<IdentifierId> ancestors) -> FixedLayoutId {
    return build_soa_layout(context, schema, schemas, true, prefix, std::move(ancestors));
}

} // namespace lispb::codegen::detail
