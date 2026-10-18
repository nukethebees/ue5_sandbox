#pragma once

#include <ioj/layout/analyzer/access_operation.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::schema {
class TypeGraph;
}

namespace ioj::layout {

struct EnumDomainAnalysis;
struct EnumTargetComparison;
struct IntegerScalarAnalysis;
struct IntegerScalarCapacityComparison;
struct OptionalSentinelAnalysis;
struct OptionalPresenceBitAnalysis;
struct OptionalEncodingComparison;
struct LinearQuantizedAnalysis;
struct LinearQuantizedComparison;
struct FixedPointAnalysis;
struct MiniFloatAnalysis;
struct IntegerVarintAnalysis;
struct IntegerVarintComparison;
struct IntegerVarintDistributionEntry;
struct IntegerVarintDistributionAnalysis;
struct IntegerVarintDistributionComparison;
struct RelationshipTargetFacts;
struct PackedAnalysis;
struct PackedTargetComparison;
struct RecordAnalysis;
struct RecordTargetComparison;
struct UnionAnalysis;
struct UnionTargetComparison;
struct UnionDistributionEntry;
struct UnionDistributionAnalysis;
struct UnionDistributionComparison;
struct TaggedUnionAnalysis;
struct TaggedUnionTargetComparison;
struct TaggedUnionDistributionEntry;
struct TaggedUnionDistributionAnalysis;
struct TaggedUnionDistributionComparison;
struct AccessIntent;
struct PackedAccessAnalysis;
struct PackedAccessComparison;
struct RecordAccessAnalysis;
struct RecordAccessComparison;
struct SoaAnalysis;
struct SoaTargetComparison;
struct SoaAccessAnalysis;
struct RecordSoaAccessComparison;
struct SoaAccessComparison;
class AbiProfile;
struct ResolvedVariant;

class Analyzer {
  public:
    static auto analyze_enum(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId type,
                             AbiProfile const& abi,
                             std::uint64_t element_count = 1) -> EnumDomainAnalysis;
    static auto compare_enum_targets(EnumDomainAnalysis const& first,
                                     EnumDomainAnalysis const& second) -> EnumTargetComparison;
    static auto
        analyze_integer_scalar(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               std::span<RelationshipTargetFacts const> relationship_targets = {})
            -> IntegerScalarAnalysis;
    static auto compare_integer_scalar_capacity(
        lispb::schema::TypeGraph const& types,
        lispb::schema::TypeId type,
        std::span<RelationshipTargetFacts const> first_relationship_targets,
        std::span<RelationshipTargetFacts const> second_relationship_targets)
        -> IntegerScalarCapacityComparison;
    static auto analyze_optional_sentinel(lispb::schema::TypeGraph const& types,
                                          lispb::schema::TypeId type,
                                          std::uint64_t element_count = 1)
        -> OptionalSentinelAnalysis;
    static auto analyze_optional_presence_bit(lispb::schema::TypeGraph const& types,
                                              lispb::schema::TypeId type,
                                              std::uint64_t element_count = 1)
        -> OptionalPresenceBitAnalysis;
    static auto compare_optional_encodings(lispb::schema::TypeGraph const& types,
                                           lispb::schema::TypeId first,
                                           lispb::schema::TypeId second,
                                           std::uint64_t element_count = 1)
        -> OptionalEncodingComparison;
    static auto analyze_linear_quantized(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId type) -> LinearQuantizedAnalysis;
    static auto compare_linear_quantized(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId first,
                                         lispb::schema::TypeId second,
                                         std::uint64_t element_count = 1)
        -> LinearQuantizedComparison;
    static auto analyze_fixed_point(lispb::schema::TypeGraph const& types,
                                    lispb::schema::TypeId type,
                                    std::uint64_t element_count = 1) -> FixedPointAnalysis;
    static auto analyze_mini_float(lispb::schema::TypeGraph const& types,
                                   lispb::schema::TypeId type,
                                   std::uint64_t element_count = 1) -> MiniFloatAnalysis;
    static auto analyze_integer_varint(lispb::schema::TypeGraph const& types,
                                       lispb::schema::TypeId type,
                                       std::uint64_t element_count = 1) -> IntegerVarintAnalysis;
    static auto compare_integer_varint(lispb::schema::TypeGraph const& types,
                                       lispb::schema::TypeId first,
                                       lispb::schema::TypeId second,
                                       std::uint64_t element_count = 1) -> IntegerVarintComparison;
    static auto
        analyze_integer_varint_distribution(lispb::schema::TypeGraph const& types,
                                            lispb::schema::TypeId type,
                                            std::span<IntegerVarintDistributionEntry const> entries,
                                            std::uint64_t selected_element_count = 1)
            -> IntegerVarintDistributionAnalysis;
    static auto
        compare_integer_varint_distribution(lispb::schema::TypeGraph const& types,
                                            lispb::schema::TypeId first,
                                            lispb::schema::TypeId second,
                                            std::span<IntegerVarintDistributionEntry const> entries,
                                            std::uint64_t selected_element_count = 1)
            -> IntegerVarintDistributionComparison;
    static auto analyze_packed(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               ResolvedVariant const& variant,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1,
                               std::span<RelationshipTargetFacts const> relationship_targets = {})
        -> PackedAnalysis;
    static auto compare_packed_targets(PackedAnalysis const& first, PackedAnalysis const& second)
        -> PackedTargetComparison;
    static auto analyze_packed_access(PackedAnalysis const& packed,
                                      std::span<AccessIntent const> accesses,
                                      AbiProfile const& abi,
                                      std::uint64_t multiplicity = 1) -> PackedAccessAnalysis;
    static auto compare_packed_access(PackedAccessAnalysis const& first,
                                      PackedAccessAnalysis const& second) -> PackedAccessComparison;
    static auto analyze_record(lispb::schema::TypeGraph const& types,
                               lispb::schema::TypeId type,
                               AbiProfile const& abi,
                               std::uint64_t element_count = 1) -> RecordAnalysis;
    static auto compare_record_targets(RecordAnalysis const& first, RecordAnalysis const& second)
        -> RecordTargetComparison;
    static auto analyze_union(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId type,
                              AbiProfile const& abi,
                              std::uint64_t element_count = 1) -> UnionAnalysis;
    static auto compare_union_targets(UnionAnalysis const& first, UnionAnalysis const& second)
        -> UnionTargetComparison;
    static auto analyze_union_distribution(UnionAnalysis const& analysis,
                                           std::span<UnionDistributionEntry const> entries,
                                           std::uint64_t selected_element_count)
        -> UnionDistributionAnalysis;
    static auto compare_union_distributions(UnionDistributionAnalysis const& first,
                                            UnionDistributionAnalysis const& second)
        -> UnionDistributionComparison;
    static auto analyze_tagged_union(lispb::schema::TypeGraph const& types,
                                     lispb::schema::TypeId type,
                                     AbiProfile const& abi,
                                     std::uint64_t element_count = 1) -> TaggedUnionAnalysis;
    static auto compare_tagged_union_targets(TaggedUnionAnalysis const& first,
                                             TaggedUnionAnalysis const& second)
        -> TaggedUnionTargetComparison;
    static auto
        analyze_tagged_union_distribution(TaggedUnionAnalysis const& tagged_union,
                                          std::span<TaggedUnionDistributionEntry const> entries,
                                          std::uint64_t selected_element_count)
            -> TaggedUnionDistributionAnalysis;
    static auto compare_tagged_union_distributions(TaggedUnionDistributionAnalysis const& first,
                                                   TaggedUnionDistributionAnalysis const& second)
        -> TaggedUnionDistributionComparison;
    static auto analyze_record_access(RecordAnalysis const& record,
                                      std::span<AccessIntent const> accesses,
                                      AbiProfile const& abi,
                                      std::uint64_t multiplicity = 1) -> RecordAccessAnalysis;
    static auto analyze_record_access(RecordAnalysis const& record,
                                      std::span<std::string const> member_names,
                                      AbiProfile const& abi,
                                      AccessOperation operation = AccessOperation::read,
                                      std::uint64_t multiplicity = 1) -> RecordAccessAnalysis;
    static auto analyze_record_member_access(RecordAnalysis const& record,
                                             std::string_view member_name,
                                             AbiProfile const& abi,
                                             AccessOperation operation = AccessOperation::read,
                                             std::uint64_t multiplicity = 1)
        -> RecordAccessAnalysis;
    static auto compare_record_access(RecordAccessAnalysis const& first,
                                      RecordAccessAnalysis const& second) -> RecordAccessComparison;
    static auto analyze_soa(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId type,
                            ResolvedVariant const& variant,
                            AbiProfile const& abi,
                            std::uint64_t default_capacity,
                            SoaAllocationStrategy allocation_strategy =
                                SoaAllocationStrategy::separate_columns) -> SoaAnalysis;
    static auto compare_soa_targets(SoaAnalysis const& first, SoaAnalysis const& second)
        -> SoaTargetComparison;
    static auto derive_relationship_target_facts(
        lispb::schema::TypeGraph const& types,
        ResolvedVariant const& variant,
        AbiProfile const& abi,
        std::uint64_t default_capacity,
        SoaAllocationStrategy allocation_strategy = SoaAllocationStrategy::separate_columns)
        -> std::vector<RelationshipTargetFacts>;
    static auto analyze_soa_access(SoaAnalysis const& soa,
                                   std::span<AccessIntent const> accesses,
                                   AbiProfile const& abi,
                                   std::uint64_t element_count,
                                   std::uint64_t multiplicity = 1) -> SoaAccessAnalysis;
    static auto analyze_soa_access(SoaAnalysis const& soa,
                                   std::span<std::string const> column_names,
                                   AbiProfile const& abi,
                                   std::uint64_t element_count,
                                   AccessOperation operation = AccessOperation::read,
                                   std::uint64_t multiplicity = 1) -> SoaAccessAnalysis;
    static auto compare_record_soa_access(RecordAccessAnalysis const& record,
                                          SoaAccessAnalysis const& soa)
        -> RecordSoaAccessComparison;
    static auto compare_soa_access(SoaAccessAnalysis const& first, SoaAccessAnalysis const& second)
        -> SoaAccessComparison;
};

}
