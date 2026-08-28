#include "fixed_soa_internal.h"
#include "lowering_utils.h"

#include <stdexcept>
#include <utility>

namespace codegen::detail {

auto fixed_leaf_argument(FixedLeaf const& leaf) -> std::string {
    return join(leaf.path, "_");
}

auto build_fixed_layout(SoaSchema const& schema,
                        std::map<std::string, SoaSchema const*> const& schemas,
                        std::map<std::string, CppType> const& types,
                        std::vector<std::string> prefix,
                        std::set<std::string> ancestors) -> FixedLayout {
    if (!schema.fixed.has_value()) {
        throw std::invalid_argument{"SOA '" + schema.name + "' has no fixed configuration"};
    }
    if (!ancestors.insert(schema.name).second) {
        throw std::invalid_argument{"Fixed SOA schema cycle at '" + schema.name + "'"};
    }
    auto resolved{resolve_members(schema, types)};
    FixedLayout result{.schema = &schema};
    for (std::size_t index{0}; index < schema.members.size(); ++index) {
        auto const& member_schema{schema.members[index]};
        auto member_path{prefix};
        member_path.push_back(member_schema.name);
        FixedMemberLayout member{
            .schema = &member_schema,
            .member = resolved[index],
        };
        if (member_schema.kind == SoaMemberKind::array) {
            member.leaves.push_back(FixedLeaf{member_path, resolved[index].element_type});
        } else {
            if (!member_schema.fixed_schema.has_value()) {
                throw std::invalid_argument{"Fixed SOA '" + schema.name + "' nested member '" +
                                            member_schema.name + "' has no fixed_schema"};
            }
            auto const found{schemas.find(*member_schema.fixed_schema)};
            if (found == schemas.end() || !found->second->fixed.has_value()) {
                throw std::invalid_argument{"Unknown fixed nested schema: " +
                                            *member_schema.fixed_schema};
            }
            auto nested{build_fixed_layout(
                *found->second, schemas, types, member_path, ancestors)};
            member.leaves = nested.leaves;
            member.nested_storage_name = nested.schema->fixed->storage_name;
        }
        result.leaves.insert(result.leaves.end(), member.leaves.begin(), member.leaves.end());
        result.members.push_back(std::move(member));
    }
    return result;
}

} // namespace codegen::detail
