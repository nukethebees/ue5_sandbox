#pragma once
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/planner_type.hpp>
#include <ioj/layout/variant_id.hpp>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
namespace ioj::layout {
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
    std::vector<std::pair<VariantId, PackedAnalysis>> packed_variants;
    std::optional<SoaAnalysis> baseline_soa;
    std::optional<SoaAnalysis> active_soa;
    std::optional<SoaTargetComparison> soa_target_comparison;
    std::optional<SoaAccessAnalysis> soa_access_analysis;
    std::optional<SoaAccessComparison> soa_target_access_comparison;
    std::optional<RecordSoaAccessComparison> record_soa_access_comparison;
    std::optional<SoaAccessComparison> soa_access_comparison;
    std::vector<std::pair<VariantId, SoaAnalysis>> soa_variants;
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

}
