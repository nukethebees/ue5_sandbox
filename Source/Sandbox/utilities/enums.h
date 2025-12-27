#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"

namespace ml {
template <typename Enum>
concept HasMaxEnumValue = requires { Enum::MAX; };

template <typename Enum, auto MAX_VALUE = Enum::MAX>
Enum get_next(Enum current) {
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
}
