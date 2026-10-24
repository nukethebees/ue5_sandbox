#pragma once

#include <ioj/sim/fighters/band_error.h>

#include <expected>

namespace ioj::sim {
class AttackDistanceBand {
  public:
    constexpr AttackDistanceBand() = default;

    auto operator==(AttackDistanceBand const&) const -> bool = default;

    [[nodiscard]] static auto make(float minimum, float desired, float maximum)
        -> std::expected<AttackDistanceBand, BandError>;

    [[nodiscard]] constexpr auto minimum() const noexcept -> float { return minimum_; }
    [[nodiscard]] constexpr auto desired() const noexcept -> float { return desired_; }
    [[nodiscard]] constexpr auto maximum() const noexcept -> float { return maximum_; }
  private:
    float minimum_{0.4f};
    float desired_{0.5f};
    float maximum_{0.6f};
};
}
