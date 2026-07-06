#pragma once

#include "CoreMinimal.h"

#include <compare>
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
