#pragma once

#include "CoreMinimal.h"

#include "generation_index.generated.h"

USTRUCT()
struct FGenerationIndex {
    GENERATED_BODY()

    static constexpr int32 INDEX_NONE{-1};

    UPROPERTY()
    int32 index{INDEX_NONE};

    UPROPERTY()
    int32 generation{INDEX_NONE};

    [[nodiscard]] bool is_valid() const noexcept;
};
