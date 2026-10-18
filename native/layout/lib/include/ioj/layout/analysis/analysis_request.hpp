#pragma once
#include <ioj/layout/access_set.hpp>
#include <ioj/layout/analyzer/integer_varint_distribution_entry.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/analyzer/tagged_union_distribution_entry.hpp>
#include <ioj/layout/analyzer/union_distribution_entry.hpp>
#include <ioj/layout/variant_id.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <span>
namespace ioj::layout {
class AbiProfile;
struct AnalysisRequest {
    lispb::schema::TypeId type{};
    AbiProfile const* abi{};
    VariantId variant{};
    std::uint64_t element_count{1};
    std::uint64_t default_capacity{65536};
    std::uint64_t multiplicity{1};
    SoaAllocationStrategy allocation_strategy{SoaAllocationStrategy::separate_columns};
    AccessSet const* accesses{};
    bool distribution_requested{};
    std::span<IntegerVarintDistributionEntry const> varint_distribution{};
    std::span<UnionDistributionEntry const> union_distribution{};
    std::span<TaggedUnionDistributionEntry const> tagged_union_distribution{};
};
}
