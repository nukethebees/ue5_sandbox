#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"
#include "UObject/Class.h"

#include "Sandbox/utilities/string.h"

namespace ml {
template <typename Enum>
concept HasMaxEnumValue = requires { Enum::MAX; };

template <typename Enum, auto MAX_VALUE = Enum::MAX>
auto get_next(Enum current) -> Enum {
    auto const next{std::to_underlying(current) + 1};
    static constexpr auto MAX{std::to_underlying(MAX_VALUE)};

    using Underlying = std::underlying_type_t<Enum>;
    return (next >= MAX) ? static_cast<Enum>(Underlying{0}) : static_cast<Enum>(next);
}

template <typename Enum>
    requires std::is_scoped_enum_v<Enum>
auto make_unhandled_enum_case_warning(Enum value) -> FString {
    return FString::Printf(TEXT("Unhandled enum case: %s"), *UEnum::GetValueAsString(value));
}

// For the string Foo::Bar, return Bar
template <typename Enum>
    requires std::is_scoped_enum_v<Enum>
auto to_string_without_type_prefix(Enum value) -> FString {
    return ml::without_class_prefix(UEnum::GetValueAsString(value));
}
}
