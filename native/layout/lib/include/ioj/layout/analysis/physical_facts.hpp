#pragma once

#include <ioj/layout/fact_origin.hpp>

#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace ioj::layout {

// Store analysis evidence in AnalysisStorage::strings alongside the numeric facts.
struct PhysicalFacts {
    auto operator==(PhysicalFacts const&) const -> bool = default;

    std::uint64_t size_bytes{};
    std::uint64_t alignment_bytes{};
    std::optional<bool> integer_signed{};
    std::optional<std::uint32_t> unsigned_value_bits{};
    lispb::IdentifierId provenance{};
    FactOrigin origin{FactOrigin::unspecified};
};

}
