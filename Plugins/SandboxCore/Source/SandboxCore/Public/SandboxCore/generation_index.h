#pragma once

#include "CoreMinimal.h"

#include "generation_index.generated.h"

USTRUCT()
struct SANDBOXCORE_API FGenerationIndex {
    GENERATED_BODY()

    static constexpr int32 INDEX_NONE{-1};

    FGenerationIndex() = default;
    FGenerationIndex(int32 index, int32 generation)
        : index(index)
        , generation(generation) {}

    bool operator==(FGenerationIndex const&) const noexcept = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
    auto to_string() const -> FString;

    UPROPERTY(VisibleAnywhere)
    int32 index{INDEX_NONE};

    UPROPERTY(VisibleAnywhere)
    int32 generation{INDEX_NONE};
};
