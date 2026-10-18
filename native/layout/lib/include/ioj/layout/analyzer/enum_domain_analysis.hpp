#pragma once

#include <ioj/layout/analyzer/enum_backing_aggregate_analysis.hpp>
#include <ioj/layout/analyzer/enum_code_value.hpp>
#include <ioj/layout/analyzer/enumerator_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct EnumDomainAnalysis {
    lispb::schema::TypeId type{};
    std::string backing_type{};
    std::optional<TypeFacts> backing_facts{};
    std::optional<std::uint64_t> backing_bits{};
    std::uint64_t live_value_count{};
    std::uint64_t reserved_value_count{};
    std::optional<EnumCodeValue> minimum_value{};
    std::optional<EnumCodeValue> maximum_value{};
    std::optional<bool> signed_domain{};
    std::optional<bool> declared_signedness{};
    std::optional<std::uint32_t> minimum_required_bits{};
    std::optional<std::uint32_t> declared_bit_width{};
    std::optional<std::uint32_t> effective_bit_width{};
    std::optional<bool> semantic_width_can_represent_domain{};
    std::optional<std::uint64_t> unused_semantic_codes{};
    std::optional<bool> backing_can_represent_domain{};
    std::optional<std::uint64_t> unused_backing_codes{};
    std::vector<EnumeratorAnalysis> enumerators{};
    std::vector<Diagnostic> diagnostics{};
    EnumBackingAggregateAnalysis aggregate{};
};

}
