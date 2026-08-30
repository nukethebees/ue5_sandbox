#pragma once

#include <array>
#include <cstddef>
#include <utility>

template <typename T, std::size_t Size>
class TStaticArray {
  public:
    constexpr TStaticArray() = default;

    template <typename... Args>
        requires(sizeof...(Args) == Size)
    constexpr TStaticArray(Args&&... args) : values_{std::forward<Args>(args)...} {}

    constexpr auto operator[](std::size_t const index) -> T& { return values_[index]; }
    constexpr auto operator[](std::size_t const index) const -> T const& {
        return values_[index];
    }

  private:
    std::array<T, Size> values_{};
};
