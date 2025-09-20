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
    constexpr auto size() const noexcept { return N; }

    constexpr StringLiteralWrapper() = default;

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

    template <typename OtherCharT, std::size_t M>
    constexpr auto concatenate(StringLiteralWrapper<OtherCharT, M> const& other) const {
        // -1 to avoid double null
        constexpr std::size_t new_size{N + M - 1};

        StringLiteralWrapper<CharType, new_size> result{};

        for (std::size_t i = 0; i < N - 1; ++i) {
            result.data_[i] = data_[i];
        }
        for (std::size_t i = 0; i < M; ++i) {
            if constexpr (std::is_same_v<OtherCharT, CharType>) {
                result.data_[i + N - 1] = other.data_[i];
            } else {
                result.data_[i + N - 1] = static_cast<CharType>(other.data_[i]);
            }
        }

        return result;
    }

    constexpr bool operator==(StringLiteralWrapper const&) const = default;

    std::array<CharType, N> data_;
};
}

template <std::size_t N>
using StaticTCharString = ml::StringLiteralWrapper<TCHAR, N>;
