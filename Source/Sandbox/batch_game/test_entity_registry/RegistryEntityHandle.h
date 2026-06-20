#pragma once

#include "CoreMinimal.h"

#include "RegistryEntityHandle.generated.h"

USTRUCT()
struct FRegistryEntityHandle {
    GENERATED_BODY()

    static constexpr int32 INDEX_NONE{-1};

    FRegistryEntityHandle() = default;
    FRegistryEntityHandle(int32 index, int32 generation)
        : index(index)
        , generation(generation) {}

    bool operator==(FRegistryEntityHandle const&) const noexcept = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
    [[nodiscard]] auto is_null() const noexcept -> bool;
    auto to_string() const -> FString;
    void reset();

    UPROPERTY(VisibleAnywhere)
    int32 index{INDEX_NONE};

    UPROPERTY(VisibleAnywhere)
    int32 generation{INDEX_NONE};
};

inline void FRegistryEntityHandle::reset() {
    index = INDEX_NONE;
    generation = INDEX_NONE;
}
inline auto FRegistryEntityHandle::is_null() const noexcept -> bool {
    return (index == INDEX_NONE) && (generation == INDEX_NONE);
}
