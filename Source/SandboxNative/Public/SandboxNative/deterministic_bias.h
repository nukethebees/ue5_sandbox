#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

struct FRegistryEntityHandle;

namespace ml {
struct FDeterministicBiases {
    uint32 integral;
    float floating;
};

namespace deterministic_bias_detail {
constexpr uint32 mix_multiplier_1{0x7feb352du};
constexpr uint32 mix_multiplier_2{0x846ca68bu};
constexpr uint32 first_input_seed{0x9e3779b9u};
constexpr uint32 second_input_seed{0x85ebca6bu};
constexpr uint32 float_seed{0xc2b2ae35u};
constexpr int32 float_mantissa_shift{8};
constexpr float float_normalisation{1.f / 16'777'216.f};

constexpr auto mix(uint32 value) noexcept -> uint32 {
    value ^= value >> 16;
    value *= mix_multiplier_1;
    value ^= value >> 15;
    value *= mix_multiplier_2;
    value ^= value >> 16;
    return value;
}
}

constexpr auto make_deterministic_biases(int32 const first, int32 const second) noexcept
    -> FDeterministicBiases {
    auto const first_bits{static_cast<uint32>(first)};
    auto const second_bits{static_cast<uint32>(second)};
    auto const combined{
        deterministic_bias_detail::mix(first_bits ^ deterministic_bias_detail::first_input_seed) ^
        deterministic_bias_detail::mix(second_bits ^ deterministic_bias_detail::second_input_seed)};
    auto const integral{deterministic_bias_detail::mix(combined)};
    auto const float_bits{deterministic_bias_detail::mix(combined ^ integral ^
                                                         deterministic_bias_detail::float_seed)};
    auto const floating{
        static_cast<float>(float_bits >> deterministic_bias_detail::float_mantissa_shift) *
        deterministic_bias_detail::float_normalisation};

    return {integral, floating};
}

auto SANDBOXNATIVE_API make_deterministic_biases(TConstArrayView<int32> const first,
                                                 TConstArrayView<int32> const second,
                                                 TArrayView<uint32> const integral_out,
                                                 TArrayView<float> const floating_out) noexcept
    -> void;

auto SANDBOXNATIVE_API
    make_deterministic_biases(TConstArrayView<FRegistryEntityHandle> const handles,
                              TArrayView<uint32> const integral_out,
                              TArrayView<float> const floating_out) noexcept -> void;
}
