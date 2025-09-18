#pragma once

#include <type_traits>
#include <utility>

template <typename Enum>
concept HasMaxEnumValue = requires { Enum::MAX; };

template <typename Enum, auto MAX_VALUE = Enum::MAX>
Enum get_next(Enum current) {
    auto const next{std::to_underlying(current) + 1};
    static constexpr auto MAX{std::to_underlying(MAX_VALUE)};

    using Underlying = std::underlying_type_t<Enum>;
    return (next >= MAX) ? static_cast<Enum>(Underlying{0}) : static_cast<Enum>(next);
}
