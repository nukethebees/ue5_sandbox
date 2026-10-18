#pragma once
#include <ioj/layout/analyzer/access_operation.hpp>
#include <ioj/layout/analyzer/soa_allocation_strategy.hpp>
#include <ioj/layout/planner_selection.hpp>
#include <ioj/layout/variant_id.hpp>
#include <ioj/layout/workspace.hpp>

#include <lispb/schema/type_identity.h>

#include <cstdint>
#include <optional>
namespace ioj::layout {
struct PlannerAnalysisInputs {
    LayoutWorkspace workspace;
    PlannerSelection selection;
    AccessOperation access_operation{AccessOperation::read};
    std::uint64_t access_multiplicity{1};
    SoaAllocationStrategy soa_allocation_strategy{SoaAllocationStrategy::separate_columns};
    VariantId comparison_a_variant_id{};
    VariantId comparison_b_variant_id{};
    bool comparison_b_follows_active{true};
    std::optional<lispb::schema::TypeIdentity> quantized_comparison_type;
    std::optional<lispb::schema::TypeIdentity> varint_comparison_type;
    std::optional<lispb::schema::TypeIdentity> optional_comparison_type;
};

}
