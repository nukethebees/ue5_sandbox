#pragma once

#include <array>

#include "CoreMinimal.h"

namespace ml {
template <typename CharT, std::size_t N>
struct StringLiteralWrapper {
    using CharType = CharT;

    std::array<CharT, N> data;

    constexpr StringLiteralWrapper(CharT const (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }

    constexpr bool operator==(StringLiteralWrapper const&) const = default;
};
}

template <std::size_t N>
using StaticTCharString = ml::StringLiteralWrapper<TCHAR, N>;
