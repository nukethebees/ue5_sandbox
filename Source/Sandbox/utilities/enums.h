#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

#include <type_traits>
#include <utility>

#include "Sandbox/utilities/string.h"

namespace ml {
template <typename T>
struct EnumCountTrait {
    static constexpr T count{T::COUNT};
    static constexpr auto count_value{std::to_underlying(count)};
};

template <typename Enum>
auto get_next(Enum current) -> Enum {
    using Underlying = std::underlying_type_t<Enum>;
    auto const next{std::to_underlying(current) + 1};

    return (next >= EnumCountTrait<Enum>::count_value) ? static_cast<Enum>(Underlying{0})
                                                       : static_cast<Enum>(next);
}

template <typename Enum>
auto get_previous(Enum current) -> Enum {
    return (current == static_cast<Enum>(0))
             ? static_cast<Enum>(EnumCountTrait<Enum>::count_value - 1)
             : static_cast<Enum>(std::to_underlying(current) - 1);
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
    auto const* const enum_ptr{StaticEnum<Enum>()};
    return enum_ptr->GetNameStringByValue(static_cast<int64>(value));
}
}
