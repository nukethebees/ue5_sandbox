#pragma once

#include <sandbox/core/enum_traits.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ml {
template <typename Enum, typename T>
    requires std::is_enum_v<Enum>
class EnumArray {
  public:
    constexpr EnumArray() = default;
    constexpr explicit EnumArray(std::array<T, enum_count<Enum>()> elements)
        : elements_{std::move(elements)} {}

    constexpr auto operator[](Enum const key) noexcept -> T& {
        return elements_[static_cast<std::size_t>(key)];
    }
    constexpr auto operator[](Enum const key) const noexcept -> T const& {
        return elements_[static_cast<std::size_t>(key)];
    }

    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t {
        return enum_count<Enum>();
    }
  private:
    std::array<T, enum_count<Enum>()> elements_{};
};
}
