#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ioj {
struct Name {};
struct Display {};
struct Serialized {};

template <typename Char, std::unsigned_integral Offset, std::size_t N>
struct PackedStrings {
    // Values must identify a row in the generated, zero-based enum domain.
    template <typename E>
        requires std::is_enum_v<E>
    [[nodiscard]] constexpr auto operator()(E value) const noexcept
        -> std::basic_string_view<Char> {
        auto const index{std::to_underlying(value)};
        assert(([&] { return std::cmp_greater_equal(index, 0) && std::cmp_less(index, N - 1); }()));
        auto const begin{offsets[index]};
        return {data + begin, static_cast<std::size_t>(offsets[index + 1] - begin - 1)};
    }

    static_assert(N > 0);

    Char const* data;
    std::array<Offset, N> offsets;
};

template <typename Char, typename Offset, std::size_t N>
PackedStrings(Char const*, std::array<Offset, N>) -> PackedStrings<Char, Offset, N>;

template <typename Enum, typename Category>
inline constexpr auto enum_strings = nullptr;

template <typename Enum, typename Category>
concept HasEnumStrings =
    std::is_enum_v<Enum> && requires(Enum value) { enum_strings<Enum, Category>(value); };

template <typename Category, typename Enum>
    requires HasEnumStrings<Enum, Category>
[[nodiscard]] constexpr auto enum_string(Enum value) noexcept {
    return enum_strings<Enum, Category>(value);
}

template <typename Enum, typename Category>
concept NarrowEnumStrings = requires(Enum value) {
    { enum_string<Category>(value) } -> std::same_as<std::string_view>;
};

template <typename Enum>
    requires NarrowEnumStrings<Enum, Name>
[[nodiscard]] constexpr auto to_string(Enum value) noexcept -> std::string_view {
    return enum_string<Name>(value);
}

template <typename Enum>
    requires NarrowEnumStrings<Enum, Display>
[[nodiscard]] constexpr auto to_display_string(Enum value) noexcept -> std::string_view {
    return enum_string<Display>(value);
}

template <typename Enum>
    requires NarrowEnumStrings<Enum, Serialized>
[[nodiscard]] constexpr auto to_serialized_string(Enum value) noexcept -> std::string_view {
    return enum_string<Serialized>(value);
}
}
