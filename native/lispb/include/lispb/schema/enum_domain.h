#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lispb::schema {

struct EnumCode {
    bool negative{};
    std::uint64_t magnitude{};

    auto operator==(EnumCode const&) const -> bool = default;
};

struct EnumDomainInput {
    std::string name;
    std::optional<std::string> explicit_value;
    bool reserved{};
};

enum class EnumDomainIssueSeverity { warning, error };

struct EnumDomainIssue {
    EnumDomainIssueSeverity severity{EnumDomainIssueSeverity::warning};
    std::string message;
};

struct EnumDomainValue {
    std::string name;
    bool reserved{};
    std::optional<EnumCode> code;
};

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
auto format_enum_code(EnumCode value) -> std::string;

} // namespace lispb::schema
