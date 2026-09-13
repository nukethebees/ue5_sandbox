#pragma once

#include <sandbox/simulation/rotator_types.h>
#include <sandbox/simulation/rotators3f.h>

#include <CoreMinimal.h>
#include <SandboxCore/soa_rotators.h>

#include <cstddef>

namespace ml {
inline auto to_native(FRotator3f const value) noexcept -> simulation::Rotator3f {
    return {.pitch = value.Pitch, .yaw = value.Yaw, .roll = value.Roll};
}

inline auto to_native(FRotatorsf::ConstView const view) noexcept
    -> simulation::Rotators3fConstView {
    auto const count{static_cast<std::size_t>(view.num())};
    return {{view.pitches.GetData(), count},
            {view.yaws.GetData(), count},
            {view.rolls.GetData(), count}};
}

inline auto to_native(FRotatorsf::View const view) noexcept -> simulation::Rotators3fView {
    auto const count{static_cast<std::size_t>(view.num())};
    return {{view.pitches.GetData(), count},
            {view.yaws.GetData(), count},
            {view.rolls.GetData(), count}};
}

inline auto to_unreal(simulation::Rotators3fConstView const view) noexcept
    -> FRotatorsf::ConstView {
    auto const count{view.num()};
    return {{view.pitches.data(), count}, {view.yaws.data(), count}, {view.rolls.data(), count}};
}
} // namespace ml
