#pragma once

#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/resolved_variant.hpp>

#include <lispb/schema/type_graph.h>
#include <lispb/schema/type_identity.h>

#include <cstdint>
#include <vector>

namespace ioj::layout {
struct ExternalTypeAnalysis {
    bool semantics_known{};
    PhysicalFactsResult physical{};
    std::vector<lispb::schema::TypeIdentity> blocked_declarations{};
};

auto analyze_external_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId type,
                           ResolvedVariant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t capacity,
                           SoaAllocationStrategy allocation_strategy) -> ExternalTypeAnalysis;
}
