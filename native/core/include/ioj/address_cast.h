#pragma once

#include <concepts>
#include <cstdint>

namespace ml {
template <std::integral Result = std::uintptr_t, typename T>
[[nodiscard]] auto address_cast(T* const pointer) noexcept -> Result {
    if constexpr (std::same_as<Result, std::uintptr_t>) {
        return reinterpret_cast<std::uintptr_t>(pointer);
    } else {
        return static_cast<Result>(reinterpret_cast<std::uintptr_t>(pointer));
    }
}
}
