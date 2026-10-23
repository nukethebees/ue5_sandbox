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
    using DeclarationId = lispb::schema::DeclarationId;
    template <typename T>
    using DeclIdMap = std::map<DeclarationId, T>;
    auto operator==(AnalysisSettings const&) const -> bool = default;

    void erase_declaration(DeclarationId declaration);
    void rename_member(DeclarationId declaration, std::string_view before, std::string_view after);
    void erase_member(DeclarationId declaration, std::string_view name);

    AbiProfile abi{AbiProfile::host_common()};
    std::optional<AbiProfile> comparison_abi{};
    std::uint64_t element_count{1};
    std::uint64_t default_soa_capacity{65536};
    std::uint64_t access_multiplicity{1};
    AccessOperation access_operation{AccessOperation::read};
    DeclIdMap<AccessSet> access_sets{};
    DeclIdMap<std::vector<IntegerVarintDistributionEntry>> varint_distributions{};
    DeclIdMap<std::vector<UnionDistributionEntry>> union_distributions{};
    DeclIdMap<std::vector<TaggedUnionDistributionEntry>> tagged_union_distributions{};
};
}
