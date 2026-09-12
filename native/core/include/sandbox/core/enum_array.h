#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

namespace ml {
template <typename Enum, typename T, std::size_t Count>
    requires std::is_enum_v<Enum> && (Count > 0)
class EnumArray {
  public:
    constexpr auto operator[](Enum const key) noexcept -> T& {
        return elements_[static_cast<std::size_t>(key)];
    }
    constexpr auto operator[](Enum const key) const noexcept -> T const& {
        return elements_[static_cast<std::size_t>(key)];
    }

    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return Count; }
  private:
    std::array<T, Count> elements_{};
};
}
