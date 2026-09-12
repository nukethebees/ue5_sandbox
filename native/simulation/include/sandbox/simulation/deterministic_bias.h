#pragma once

#include "sandbox/simulation/entity_handle.h"

#include <cstdint>
#include <span>

namespace ml {
struct DeterministicBiases {
    std::uint32_t integral;
    float floating;
};

namespace deterministic_bias_detail {
inline constexpr std::uint32_t mix_multiplier_1{0x7feb352du};
inline constexpr std::uint32_t mix_multiplier_2{0x846ca68bu};
inline constexpr std::uint32_t first_input_seed{0x9e3779b9u};
inline constexpr std::uint32_t second_input_seed{0x85ebca6bu};
inline constexpr std::uint32_t float_seed{0xc2b2ae35u};
inline constexpr std::int32_t float_mantissa_shift{8};
inline constexpr float float_normalisation{1.f / 16'777'216.f};

constexpr auto mix(std::uint32_t value) noexcept -> std::uint32_t {
    value ^= value >> 16;
    value *= mix_multiplier_1;
    value ^= value >> 15;
    value *= mix_multiplier_2;
    value ^= value >> 16;
    return value;
}

constexpr auto combine_inputs(std::int32_t const first, std::int32_t const second) noexcept
    -> std::uint32_t {
    auto const first_bits{static_cast<std::uint32_t>(first)};
    auto const second_bits{static_cast<std::uint32_t>(second)};
    return mix(first_bits ^ first_input_seed) ^ mix(second_bits ^ second_input_seed);
}
}

constexpr auto make_deterministic_integral_bias(std::int32_t const first,
                                                std::int32_t const second) noexcept
    -> std::uint32_t {
    return deterministic_bias_detail::mix(deterministic_bias_detail::combine_inputs(first, second));
}

constexpr auto make_deterministic_biases(std::int32_t const first,
                                         std::int32_t const second) noexcept
    -> DeterministicBiases {
    auto const combined{deterministic_bias_detail::combine_inputs(first, second)};
    auto const integral{deterministic_bias_detail::mix(combined)};
    auto const float_bits{deterministic_bias_detail::mix(combined ^ integral ^
                                                         deterministic_bias_detail::float_seed)};
    auto const floating{
        static_cast<float>(float_bits >> deterministic_bias_detail::float_mantissa_shift) *
        deterministic_bias_detail::float_normalisation};

    return {integral, floating};
}

auto make_deterministic_biases(std::span<std::int32_t const> first,
                               std::span<std::int32_t const> second,
                               std::span<std::uint32_t> integral_out,
                               std::span<float> floating_out) noexcept -> bool;
auto make_deterministic_biases(std::span<FRegistryEntityHandle const> handles,
                               std::span<std::uint32_t> integral_out) noexcept -> bool;
}
