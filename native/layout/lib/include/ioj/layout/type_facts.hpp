#pragma once

#include <ioj/layout/fact_origin.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace ioj::layout {

struct TypeFacts {
    auto operator==(TypeFacts const&) const -> bool = default;

    // Complete-object sizeof, including tail padding; also the stride in an array.
    std::uint64_t size_bytes{};
    std::uint64_t alignment_bytes{};
    std::optional<bool> integer_signed;
    std::optional<std::uint32_t> unsigned_value_bits;
    std::string provenance;
    FactOrigin origin{FactOrigin::unspecified};
};

auto validate_type_facts(TypeFacts const& facts) -> std::expected<void, std::string>;

} // namespace ioj::layout
