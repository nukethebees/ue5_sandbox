#pragma once

#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_selection.hpp>
#include <ioj/layout/planner_type.hpp>

#include <lispb/schema/editable_document.h>

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ioj::layout {

struct PlannerAnalysisInputs {
    LayoutWorkspace workspace;
    PlannerSelection selection;
    AccessOperation access_operation{AccessOperation::read};
    std::uint64_t access_multiplicity{1};
    SoaAllocationStrategy soa_allocation_strategy{SoaAllocationStrategy::separate_columns};
    std::uint64_t comparison_a_variant_id{LayoutWorkspace::baseline_variant_id};
    std::uint64_t comparison_b_variant_id{LayoutWorkspace::baseline_variant_id};
    bool comparison_b_follows_active{true};
    std::optional<lispb::schema::TypeIdentity> quantized_comparison_type;
    std::optional<lispb::schema::TypeIdentity> varint_comparison_type;
    std::optional<lispb::schema::TypeIdentity> optional_comparison_type;
};

struct PlannerAnalysisResults {
    std::optional<ExternalTypeAnalysis> external_type;
    std::optional<EnumDomainAnalysis> enum_domain;
    std::optional<EnumTargetComparison> enum_target_comparison;
    std::optional<IntegerScalarAnalysis> integer_scalar_analysis;
    std::optional<IntegerScalarCapacityComparison> integer_scalar_capacity_comparison;
    std::optional<LinearQuantizedAnalysis> linear_quantized_analysis;
    std::optional<LinearQuantizedComparison> linear_quantized_comparison;
    std::optional<IntegerVarintAnalysis> integer_varint_analysis;
    std::optional<IntegerVarintComparison> integer_varint_comparison;
    std::optional<IntegerVarintDistributionAnalysis> integer_varint_distribution;
    std::optional<IntegerVarintDistributionComparison> integer_varint_distribution_comparison;
    std::optional<FixedPointAnalysis> fixed_point_analysis;
    std::optional<MiniFloatAnalysis> mini_float_analysis;
    std::optional<OptionalSentinelAnalysis> optional_sentinel_analysis;
    std::optional<OptionalPresenceBitAnalysis> optional_presence_bit_analysis;
    std::optional<OptionalEncodingComparison> optional_encoding_comparison;
    std::optional<PackedAnalysis> baseline_packed;
    std::optional<PackedAnalysis> active_packed;
    std::optional<PackedTargetComparison> packed_target_comparison;
    std::optional<PackedAccessAnalysis> packed_access_analysis;
    std::optional<PackedAccessComparison> packed_target_access_comparison;
    std::optional<PackedAccessComparison> packed_access_comparison;
    std::vector<std::pair<std::uint64_t, PackedAnalysis>> packed_variants;
    std::optional<SoaAnalysis> baseline_soa;
    std::optional<SoaAnalysis> active_soa;
    std::optional<SoaTargetComparison> soa_target_comparison;
    std::optional<SoaAccessAnalysis> soa_access_analysis;
    std::optional<SoaAccessComparison> soa_target_access_comparison;
    std::optional<RecordSoaAccessComparison> record_soa_access_comparison;
    std::optional<SoaAccessComparison> soa_access_comparison;
    std::vector<std::pair<std::uint64_t, SoaAnalysis>> soa_variants;
    std::optional<PackedAnalysis> comparison_a_packed;
    std::optional<PackedAnalysis> comparison_b_packed;
    std::optional<RecordAnalysis> record_analysis;
    std::optional<RecordTargetComparison> record_target_comparison;
    std::optional<RecordAccessAnalysis> record_access_analysis;
    std::optional<RecordAccessComparison> record_target_access_comparison;
    std::optional<UnionAnalysis> union_analysis;
    std::optional<UnionTargetComparison> union_target_comparison;
    std::optional<UnionDistributionAnalysis> union_distribution_analysis;
    std::optional<UnionDistributionComparison> union_target_distribution_comparison;
    std::optional<TaggedUnionAnalysis> tagged_union_analysis;
    std::optional<TaggedUnionTargetComparison> tagged_union_target_comparison;
    std::optional<TaggedUnionDistributionAnalysis> tagged_union_distribution_analysis;
    std::optional<TaggedUnionDistributionComparison> tagged_union_target_distribution_comparison;
    std::optional<SoaAnalysis> comparison_a_soa;
    std::optional<SoaAnalysis> comparison_b_soa;
};

class PlannerAnalysisSession {
  public:
    using DistributionWeights = std::map<std::string, std::uint64_t>;
    using DistributionTable = std::map<lispb::schema::DeclarationId, DistributionWeights>;

    explicit PlannerAnalysisSession(lispb::schema::TypeGraph types = {});

    PlannerAnalysisInputs inputs;
    auto primary_abi() const -> AbiProfile const&;
    auto comparison_abi() const -> AbiProfile const&;
    auto set_primary_abi(AbiProfile abi) -> bool;
    auto set_external_type_facts(lispb::schema::TypeId type, TypeFacts facts)
        -> std::expected<bool, std::string>;
    auto set_comparison_abi(AbiProfile abi) -> bool;
    auto union_distributions() const -> DistributionTable const&;
    auto tagged_union_distributions() const -> DistributionTable const&;
    auto union_distribution(lispb::schema::DeclarationId declaration) const
        -> DistributionWeights const*;
    auto tagged_union_distribution(lispb::schema::DeclarationId declaration) const
        -> DistributionWeights const*;
    auto set_union_distribution_weight(lispb::schema::DeclarationId declaration,
                                       std::string name,
                                       std::uint64_t weight) -> bool;
    auto set_tagged_union_distribution_weight(lispb::schema::DeclarationId declaration,
                                              std::string name,
                                              std::uint64_t weight) -> bool;
    auto clear_union_distribution(lispb::schema::DeclarationId declaration) -> bool;
    auto clear_tagged_union_distribution(lispb::schema::DeclarationId declaration) -> bool;
    void clear_distributions();
    auto set_varint_distribution(std::optional<lispb::schema::TypeIdentity> source,
                                 std::vector<IntegerVarintDistributionEntry> entries,
                                 bool rows_present) -> bool;
    auto results() const -> PlannerAnalysisResults const&;
    auto status(lispb::schema::TypeId type) -> LayoutStatus;
    void replace_types(lispb::schema::EditableSchemaDocument const& document,
                       std::optional<lispb::schema::TypeIdentity> selection);
    auto refresh(lispb::schema::EditableSchemaDocument const* document) -> bool;
    void validate_comparison_variants();
  private:
    void analyze_selected(lispb::schema::EditableSchemaDocument const* document,
                          PlannerAnalysisResults previous,
                          bool reuse_primary,
                          bool reuse_comparison_target,
                          bool reuse_variant_comparison);
    PlannerAnalysisResults results_;
    AbiProfile primary_abi_{AbiProfile::host_common()};
    AbiProfile comparison_abi_{AbiProfile::host_common()};
    std::uint64_t target_profile_revision_{};
    std::uint64_t comparison_target_profile_revision_{};
    std::optional<lispb::schema::TypeIdentity> varint_distribution_source_;
    std::vector<IntegerVarintDistributionEntry> varint_distribution_entries_;
    bool varint_distribution_rows_present_{};
    std::uint64_t varint_distribution_revision_{};
    DistributionTable union_distributions_;
    std::uint64_t union_distribution_revision_{};
    DistributionTable tagged_union_distributions_;
    std::uint64_t tagged_distribution_revision_{};
    std::map<std::uint32_t, LayoutStatus> status_cache_;
    std::vector<RelationshipTargetFacts> status_relationship_targets_;
    std::uint64_t status_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t status_profile_revision_{std::numeric_limits<std::uint64_t>::max()};
    SoaAllocationStrategy status_allocation_strategy_{SoaAllocationStrategy::separate_columns};
    std::uint64_t cached_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_target_profile_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_comparison_target_profile_revision_{
        std::numeric_limits<std::uint64_t>::max()};
    AccessOperation cached_access_operation_{AccessOperation::read};
    std::uint64_t cached_access_multiplicity_{1};
    SoaAllocationStrategy cached_soa_allocation_strategy_{SoaAllocationStrategy::separate_columns};
    std::optional<lispb::schema::TypeId> cached_type_;
    std::string cached_selected_field_;
    std::map<std::string, AccessOperation, std::less<>> cached_packed_access_fields_;
    bool cached_packed_access_set_explicit_{};
    std::map<std::string, AccessOperation, std::less<>> cached_record_access_members_;
    bool cached_record_access_set_explicit_{};
    std::map<std::string, AccessOperation, std::less<>> cached_soa_access_columns_;
    bool cached_soa_access_set_explicit_{};
    std::uint64_t cached_comparison_a_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_comparison_b_variant_id_{std::numeric_limits<std::uint64_t>::max()};
    std::optional<lispb::schema::TypeIdentity> cached_quantized_comparison_type_;
    std::optional<lispb::schema::TypeIdentity> cached_varint_comparison_type_;
    std::uint64_t cached_varint_distribution_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::optional<lispb::schema::TypeIdentity> cached_optional_comparison_type_;
    std::uint64_t cached_union_distribution_revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t cached_tagged_distribution_revision_{std::numeric_limits<std::uint64_t>::max()};
};

} // namespace ioj::layout
