#pragma once

#include <ioj/enum_traits.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ioj {
template <typename Enum, typename T>
    requires std::is_enum_v<Enum>
class EnumArray {
  public:
    constexpr EnumArray() = default;
    constexpr explicit EnumArray(T const& value) { elements_.fill(value); }
    constexpr explicit EnumArray(std::array<T, enum_count<Enum>()> elements)
        : elements_{std::move(elements)} {}

    constexpr auto operator[](Enum const key) noexcept -> T& {
        return elements_[static_cast<std::size_t>(key)];
    }
    constexpr auto operator[](Enum const key) const noexcept -> T const& {
        return elements_[static_cast<std::size_t>(key)];
    }
    auto operator==(EnumArray const&) const -> bool = default;

    constexpr void fill(T const& value) { elements_.fill(value); }
    [[nodiscard]] constexpr auto begin() noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return elements_.end(); }
    [[nodiscard]] constexpr auto end() const noexcept { return elements_.end(); }

    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t {
        return enum_count<Enum>();
    }
  private:
    std::array<T, enum_count<Enum>()> elements_{};
};
}
