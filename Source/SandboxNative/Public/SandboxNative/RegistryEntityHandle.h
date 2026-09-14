#pragma once

#include <CoreMinimal.h>
#include <Templates/TypeHash.h>

#include <ioj/sim/entity_handle.h>

#include <bit>
#include <compare>
#include <type_traits>

inline auto GetTypeHash(::ioj::sim::RegistryEntityHandle const& handle) -> uint32 {
    static_assert(sizeof(::ioj::sim::RegistryEntityHandle) == sizeof(uint64));
    static_assert(std::is_trivially_copyable_v<::ioj::sim::RegistryEntityHandle>);

    return GetTypeHash(std::bit_cast<uint64>(handle));
}

inline auto LexToString(::ioj::sim::RegistryEntityHandle const& handle) -> FString {
    if (handle.is_valid()) {
        return FString::Printf(TEXT("{%d, Gen:%d}"), handle.index, handle.generation);
    }

    return TEXT("{x, Gen:x}");
}
