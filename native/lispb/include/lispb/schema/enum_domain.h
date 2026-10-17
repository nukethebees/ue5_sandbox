#pragma once

#include <codegen/schema/enum_schema.h>
#include <lispb/schema/enum_code.h>
#include <lispb/schema/enum_domain_input.h>
#include <lispb/schema/enum_domain_issue.h>
#include <lispb/schema/enum_domain_value.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace lispb::schema {
struct EnumDomain {
    std::uint64_t live_value_count{};
    std::uint64_t reserved_value_count{};
    std::uint64_t distinct_code_count{};
    std::optional<EnumCode> minimum_value;
    std::optional<EnumCode> maximum_value;
    std::optional<bool> signed_domain;
    std::optional<std::uint32_t> minimum_required_bits;
    std::vector<EnumDomainValue> values;
    std::vector<EnumDomainIssue> issues;
};

auto analyze_enum_domain(std::span<EnumDomainInput const> values,
                         std::optional<bool> signedness = std::nullopt) -> EnumDomain;
auto analyze_enum_domain(codegen::EnumSchema const& schema) -> EnumDomain;

} // namespace lispb::schema
