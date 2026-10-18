#pragma once

#include <ioj/layout/analyzer/numeric_delta_direction.hpp>

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct NumericDelta {
    auto operator==(NumericDelta const&) const -> bool = default;

    NumericDeltaDirection direction{NumericDeltaDirection::unchanged};
    std::uint64_t magnitude{};
    std::optional<double> percentage{};
};

auto numeric_delta(std::optional<std::uint64_t> baseline, std::optional<std::uint64_t> variant)
    -> std::optional<NumericDelta>;

}
