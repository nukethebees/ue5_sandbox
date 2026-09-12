#include "sandbox/simulation/deterministic_bias.h"

namespace ml {
auto make_deterministic_biases(std::span<std::int32_t const> const first,
                               std::span<std::int32_t const> const second,
                               std::span<std::uint32_t> const integral_out,
                               std::span<float> const floating_out) noexcept -> bool {
    auto const count{first.size()};
    if (second.size() != count || integral_out.size() != count || floating_out.size() != count) {
        return false;
    }

    for (std::size_t i{0}; i < count; ++i) {
        auto const biases{make_deterministic_biases(first[i], second[i])};
        integral_out[i] = biases.integral;
        floating_out[i] = biases.floating;
    }

    return true;
}

auto make_deterministic_biases(std::span<FRegistryEntityHandle const> const handles,
                               std::span<std::uint32_t> const integral_out) noexcept -> bool {
    auto const count{handles.size()};
    if (integral_out.size() != count) {
        return false;
    }

    for (std::size_t i{0}; i < count; ++i) {
        auto const handle{handles[i]};
        integral_out[i] = make_deterministic_integral_bias(handle.index, handle.generation);
    }

    return true;
}
}
