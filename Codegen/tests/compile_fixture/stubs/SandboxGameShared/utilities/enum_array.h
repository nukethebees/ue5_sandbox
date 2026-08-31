#pragma once

#include <CoreMinimal.h>
#include <Containers/StaticArray.h>

#include <type_traits>

template <typename Enum>
struct TEnumTraits {
    static constexpr int32 count{-1};
};

template <typename Enum, typename T>
class TEnumArray {
    static_assert(std::is_enum_v<Enum>);
    static_assert(TEnumTraits<Enum>::count > 0);

  public:
    constexpr auto operator[](Enum const key) -> T& {
        return elems_[static_cast<int32>(key)];
    }

    constexpr auto operator[](Enum const key) const -> T const& {
        return elems_[static_cast<int32>(key)];
    }

    static constexpr auto size() -> int32 { return TEnumTraits<Enum>::count; }

  private:
    TStaticArray<T, TEnumTraits<Enum>::count> elems_{};
};
