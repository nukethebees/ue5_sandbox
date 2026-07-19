#pragma once

#include <CoreMinimal.h>
#include <Templates/TypeHash.h>

#include <bit>
#include <compare>
#include <type_traits>

struct SANDBOX_API FRegistryEntityHandle {
    using index_type = int32;
    using generation_type = int32;

    static constexpr index_type INDEX_NONE{-1};

    FRegistryEntityHandle() = default;
    FRegistryEntityHandle(index_type index, generation_type generation)
        : index(index)
        , generation(generation) {}

    auto operator<=>(FRegistryEntityHandle const&) const noexcept = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
    [[nodiscard]] auto is_null() const noexcept -> bool;
    auto to_string() const -> FString;
    void reset();

    friend uint32 GetTypeHash(FRegistryEntityHandle const& handle) {
        static_assert(sizeof(FRegistryEntityHandle) == sizeof(uint64));
        static_assert(std::is_trivially_copyable_v<FRegistryEntityHandle>);

        return GetTypeHash(std::bit_cast<uint64>(handle));
    }

    index_type index{INDEX_NONE};
    generation_type generation{INDEX_NONE};
};

inline void FRegistryEntityHandle::reset() {
    index = INDEX_NONE;
    generation = INDEX_NONE;
}
inline auto FRegistryEntityHandle::is_null() const noexcept -> bool {
    return (index == INDEX_NONE) && (generation == INDEX_NONE);
}

inline auto LexToString(FRegistryEntityHandle const& handle) -> FString {
    return handle.to_string();
}
