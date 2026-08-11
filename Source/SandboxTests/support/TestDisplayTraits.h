#pragma once

#include "lex_to_string.h"

#include <Sandbox/utilities/enums.h>

#include <CoreMinimal.h>
#include <Misc/StringFormatArg.h>

#include <concepts>
#include <type_traits>

namespace ml {
template <typename T>
struct TestDisplayTraits {
    static auto format_arg(T const& value) -> FStringFormatArg {
        if constexpr (std::same_as<T, bool>) {
            return FStringFormatArg{value ? TEXT("true") : TEXT("false")};
        } else if constexpr (std::convertible_to<T, TCHAR const*>) {
            return FStringFormatArg{value};
        } else if constexpr (std::integral<T>) {
            if constexpr (std::is_signed_v<T>) {
                return FStringFormatArg{static_cast<int64>(value)};
            } else {
                return FStringFormatArg{static_cast<uint64>(value)};
            }
        } else if constexpr (std::floating_point<T>) {
            return FStringFormatArg{value};
        } else if constexpr (supports_lex_to_string<T>) {
            return FStringFormatArg{LexToString(value)};
        } else {
            static_assert(is_uenum<T>, "Test display formatting is not defined for this type.");
            return FStringFormatArg{ml::to_string_without_type_prefix(value)};
        }
    }
};

template <>
struct TestDisplayTraits<FString> {
    static auto format_arg(FString const& value) -> FStringFormatArg {
        return FStringFormatArg{value};
    }
};

template <>
struct TestDisplayTraits<FVector2D> {
    static auto format_arg(FVector2D const& value) -> FStringFormatArg {
        return FStringFormatArg{value.ToString()};
    }
};

template <>
struct TestDisplayTraits<FVector3d> {
    static auto format_arg(FVector3d const& value) -> FStringFormatArg {
        return FStringFormatArg{value.ToCompactString()};
    }
};

template <typename T>
auto make_test_display_arg(T const& value) -> FStringFormatArg {
    return TestDisplayTraits<std::remove_cvref_t<T>>::format_arg(value);
}

template <typename T>
concept SoftTestDescription =
    std::same_as<std::remove_cvref_t<T>, FString> || std::convertible_to<T, TCHAR const*>;
}
