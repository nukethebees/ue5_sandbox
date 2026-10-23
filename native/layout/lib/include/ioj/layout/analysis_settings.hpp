#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/access_set.hpp>
#include <ioj/layout/analyzer/integer_varint_distribution_entry.hpp>
#include <ioj/layout/analyzer/tagged_union_distribution_entry.hpp>
#include <ioj/layout/analyzer/union_distribution_entry.hpp>

#include <lispb/schema/indices.h>

#include <map>
#include <optional>
#include <vector>

namespace ioj::layout {
struct AnalysisSettings {
    auto operator==(AnalysisSettings const&) const -> bool = default;

    void erase_declaration(lispb::schema::DeclarationId declaration);
    void rename_member(lispb::schema::DeclarationId declaration,
                       std::string_view before,
                       std::string_view after);
    void erase_member(lispb::schema::DeclarationId declaration, std::string_view name);

    AbiProfile abi{AbiProfile::host_common()};
    std::optional<AbiProfile> comparison_abi{};
    std::uint64_t element_count{1};
    std::uint64_t default_soa_capacity{65536};
    std::uint64_t access_multiplicity{1};
    AccessOperation access_operation{AccessOperation::read};
    std::map<lispb::schema::DeclarationId, AccessSet> access_sets{};
    std::map<lispb::schema::DeclarationId, std::vector<IntegerVarintDistributionEntry>>
        varint_distributions{};
    std::map<lispb::schema::DeclarationId, std::vector<UnionDistributionEntry>>
        union_distributions{};
    std::map<lispb::schema::DeclarationId, std::vector<TaggedUnionDistributionEntry>>
        tagged_union_distributions{};
};
}
