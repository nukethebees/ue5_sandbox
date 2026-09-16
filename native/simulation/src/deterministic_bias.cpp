#include "ioj/sim/deterministic_bias.h"

namespace ioj::sim {
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

auto make_deterministic_biases(std::span<EntityUniqueId const> const ids,
                               std::span<std::uint32_t> const integral_out) noexcept -> bool {
    auto const count{ids.size()};
    if (integral_out.size() != count) {
        return false;
    }

    for (std::size_t i{0}; i < count; ++i) {
        integral_out[i] =
            make_deterministic_integral_bias(static_cast<std::int32_t>(ids[i].raw_value()), 0);
    }

    return true;
}
auto make_deterministic_biases(std::span<EntityUniqueId const> const ids,
                               std::span<std::uint32_t> const integral_out,
                               std::span<float> const floating_out) noexcept -> bool {
    auto const count{ids.size()};
    if (integral_out.size() != count || floating_out.size() != count) {
        return false;
    }
    for (std::size_t i{}; i < count; ++i) {
        auto const biases{
            make_deterministic_biases(static_cast<std::int32_t>(ids[i].raw_value()), 0)};
        integral_out[i] = biases.integral;
        floating_out[i] = biases.floating;
    }
    return true;
}
}
