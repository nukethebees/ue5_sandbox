#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/relationship_target_facts.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/resolved_variant.hpp>

#include <lispb/schema/type_graph.h>

#include <cstdint>
#include <span>

namespace ioj::layout {
enum class LayoutStatus { available, unknown, error };

auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId type,
                        ResolvedVariant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity) -> LayoutStatus;
auto declaration_status(lispb::schema::TypeGraph const& types,
                        lispb::schema::TypeId type,
                        ResolvedVariant const& variant,
                        AbiProfile const& abi,
                        std::uint64_t default_capacity,
                        std::uint64_t element_count,
                        SoaAllocationStrategy allocation_strategy,
                        std::span<RelationshipTargetFacts const> relationship_targets)
    -> LayoutStatus;
}
