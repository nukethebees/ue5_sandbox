#include <ioj/layout/analyzer/numeric_delta.hpp>

#include <ioj/layout/analyzer/numeric_delta_direction.hpp>

namespace ioj::layout {

auto numeric_delta(std::optional<std::uint64_t> const baseline,
                   std::optional<std::uint64_t> const variant) -> std::optional<NumericDelta> {
    if (!baseline.has_value() || !variant.has_value()) {
        return std::nullopt;
    }
    if (*baseline == *variant) {
        return NumericDelta{.direction = NumericDeltaDirection::unchanged,
                            .magnitude = 0,
                            .percentage =
                                *baseline == 0 ? std::nullopt : std::optional<double>{0.0}};
    }
    auto const increased{*variant > *baseline};
    auto const magnitude{increased ? *variant - *baseline : *baseline - *variant};
    return NumericDelta{.direction = increased ? NumericDeltaDirection::increased
                                               : NumericDeltaDirection::decreased,
                        .magnitude = magnitude,
                        .percentage = *baseline == 0 ? std::nullopt
                                                     : std::optional<double>{
                                                           static_cast<double>(magnitude) * 100.0 /
                                                           static_cast<double>(*baseline)}};
}

}
