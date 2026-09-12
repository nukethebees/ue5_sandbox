#pragma once

#include <CoreMinimal.h>

#include <sandbox/core/enum_array.h>

#include <type_traits>

template <typename Enum>
struct TEnumTraits {
    static constexpr int32 count{-1};
};

template <typename Enum, typename T>
class TEnumArray
    : public ml::EnumArray<Enum, T, static_cast<std::size_t>(TEnumTraits<Enum>::count)> {
    static_assert(std::is_enum_v<Enum>);
    static_assert(TEnumTraits<Enum>::count > 0,
                  "TEnumArray requires enum metadata generated with enum_array: true");
  public:
    static constexpr auto size() -> int32 { return TEnumTraits<Enum>::count; }
};
