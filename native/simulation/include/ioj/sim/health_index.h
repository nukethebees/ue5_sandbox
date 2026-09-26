#pragma once

#include <compare>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace ioj::sim {
struct HealthIndex {
    using storage_type = std::uint32_t;

    inline static constexpr storage_type invalid_value{std::numeric_limits<storage_type>::max()};

    constexpr HealthIndex() noexcept = default;
    explicit constexpr HealthIndex(storage_type const value) noexcept
        : value_{value} {}

    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type { return value_; }
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return value_ != invalid_value;
    }
    [[nodiscard]] constexpr auto operator<=>(HealthIndex const&) const noexcept = default;
  private:
    storage_type value_{invalid_value};
};
static_assert(sizeof(HealthIndex) == sizeof(HealthIndex::storage_type));
static_assert(std::is_trivially_copyable_v<HealthIndex>);
static_assert(std::is_standard_layout_v<HealthIndex>);
}
