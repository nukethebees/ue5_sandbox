#pragma once

#include <ioj/component_sources.h>

namespace ioj {
// Store pitch, yaw and roll in degrees using the Unreal convention.
template <std::floating_point T>
struct Rotator {
    constexpr Rotator() = default;
    template <std::convertible_to<T> Pitch, std::convertible_to<T> Yaw, std::convertible_to<T> Roll>
    constexpr Rotator(Pitch const pitch_value, Yaw const yaw_value, Roll const roll_value)
        : pitch{static_cast<T>(pitch_value)}
        , yaw{static_cast<T>(yaw_value)}
        , roll{static_cast<T>(roll_value)} {}

    template <typename Source>
        requires EulerMembers<Source, T>
    explicit constexpr Rotator(Source const& value)
        : Rotator{value.pitch, value.yaw, value.roll} {}

    template <typename Source>
        requires EulerAccessors<Source, T>
    explicit constexpr Rotator(Source const& value)
        : Rotator{value.pitch(), value.yaw(), value.roll()} {}

    template <typename Source>
        requires UppercaseEulerMembers<Source, T>
    explicit constexpr Rotator(Source const& value)
        : Rotator{value.Pitch, value.Yaw, value.Roll} {}

    template <typename Source>
        requires UppercaseEulerAccessors<Source, T>
    explicit constexpr Rotator(Source const& value)
        : Rotator{value.Pitch(), value.Yaw(), value.Roll()} {}

    auto operator==(Rotator const&) const -> bool = default;

    T pitch{};
    T yaw{};
    T roll{};
};

using Rotator3f = Rotator<float>;
using Rotator3d = Rotator<double>;
}
