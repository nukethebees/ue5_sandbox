#pragma once

#include <ioj/enum_traits.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ioj {
template <typename T, typename... Enums>
    requires (sizeof...(Enums) > 0 && (std::is_enum_v<Enums> && ...))
class EnumArray {
  public:
    static constexpr std::size_t count{(enum_count<Enums>() * ...)};

    constexpr EnumArray() = default;
    constexpr explicit EnumArray(T const& value) { elements_.fill(value); }
    constexpr explicit EnumArray(std::array<T, count> elements)
        : elements_{std::move(elements)} {}

    constexpr auto& operator[](this auto& self, Enums const... keys) noexcept {
        return self.elements_[index(keys...)];
    }
    auto operator==(EnumArray const&) const -> bool = default;

    constexpr void fill(T const& value) { elements_.fill(value); }
    [[nodiscard]] constexpr auto begin() noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return elements_.end(); }
    [[nodiscard]] constexpr auto end() const noexcept { return elements_.end(); }

    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return count; }
  private:
    static constexpr auto index(Enums const... keys) noexcept -> std::size_t {
        std::size_t result{};
        ((result = result * enum_count<Enums>() + static_cast<std::size_t>(keys)), ...);
        return result;
    }

    std::array<T, count> elements_{};
};
}
