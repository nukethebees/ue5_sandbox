#pragma once

#include <ioj/constrained_value.h>

namespace ioj::settings {
inline constexpr auto attack_distance_band_policy{
    ConstrainedValuePolicy{[](auto const& value) constexpr noexcept {
                               return 0.f <= value.minimum && value.minimum <= value.desired &&
                                      value.desired <= value.maximum && value.maximum <= 1.f;
                           },
                           "Expected 0 <= minimum <= desired <= maximum <= 1"}};
}
