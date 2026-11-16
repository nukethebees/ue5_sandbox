#pragma once

#include <ioj/enumerator_range.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ioj {
template <typename T, auto const& Range>
class EnumRangeArray {
  public:
    using Enum = decltype(Range.first());
    using Unsigned = std::make_unsigned_t<std::underlying_type_t<Enum>>;
    static constexpr std::size_t count{
        static_cast<std::size_t>(static_cast<Unsigned>(static_cast<Unsigned>(Range.last()) -
                                                       static_cast<Unsigned>(Range.first()))) +
        1};

    constexpr EnumRangeArray() = default;
    constexpr explicit EnumRangeArray(T const& value) { elements_.fill(value); }
    constexpr explicit EnumRangeArray(std::array<T, count> elements)
        : elements_{std::move(elements)} {}

    constexpr auto& operator[](this auto& self, Enum const key) noexcept {
        return self.elements_[index(key)];
    }
    auto operator==(EnumRangeArray const&) const -> bool = default;

    constexpr void fill(T const& value) { elements_.fill(value); }
    [[nodiscard]] constexpr auto begin() noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return elements_.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return elements_.end(); }
    [[nodiscard]] constexpr auto end() const noexcept { return elements_.end(); }
    [[nodiscard]] static constexpr auto size() noexcept -> std::size_t { return count; }

  private:
    static constexpr auto index(Enum const key) noexcept -> std::size_t {
        assert(key >= Range.first() && key <= Range.last());
        return static_cast<Unsigned>(static_cast<Unsigned>(key) -
                                     static_cast<Unsigned>(Range.first()));
    }

    std::array<T, count> elements_{};
};
}
