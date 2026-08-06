#pragma once

#include "CoreMinimal.h"

namespace ml {
struct FDeterministicBiases {
    uint32 integral;
    float floating;
};

namespace deterministic_bias_detail {
constexpr auto mix(uint32 value) noexcept -> uint32 {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}
}

constexpr auto make_deterministic_biases(int32 const first, int32 const second) noexcept
    -> FDeterministicBiases {
    auto const first_bits{static_cast<uint32>(first)};
    auto const second_bits{static_cast<uint32>(second)};
    auto const combined{deterministic_bias_detail::mix(first_bits ^ 0x9e3779b9u) ^
                        deterministic_bias_detail::mix(second_bits ^ 0x85ebca6bu)};
    auto const integral{deterministic_bias_detail::mix(combined)};
    auto const float_bits{deterministic_bias_detail::mix(combined ^ integral ^ 0xc2b2ae35u)};
    constexpr auto float_scale{1.f / 16'777'216.f};
    auto const floating{static_cast<float>(float_bits >> 8) * float_scale};

    return {integral, floating};
}
}
