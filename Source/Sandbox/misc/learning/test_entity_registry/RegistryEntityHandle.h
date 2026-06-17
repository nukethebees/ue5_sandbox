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
    auto to_string() const -> FString;

    UPROPERTY(VisibleAnywhere)
    int32 index{INDEX_NONE};

    UPROPERTY(VisibleAnywhere)
    int32 generation{INDEX_NONE};
};
