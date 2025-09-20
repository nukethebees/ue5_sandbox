#pragma once

#include <array>
#include <concepts>
#include <type_traits>

#include "CoreMinimal.h"

namespace ml {
template <typename CharT, std::size_t N>
struct StringLiteralWrapper {
    using CharType = CharT;

    constexpr auto* data() const noexcept { return data_.data(); }

    template <std::integral T>
    constexpr StringLiteralWrapper(T const (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (std::is_same_v<T, CharType>) {
                data_[i] = s[i];
            } else {
                data_[i] = static_cast<CharType>(s[i]);
            }
        }
    }

    constexpr bool operator==(StringLiteralWrapper const&) const = default;

    std::array<CharType, N> data_;
};
}

template <std::size_t N>
using StaticTCharString = ml::StringLiteralWrapper<TCHAR, N>;
