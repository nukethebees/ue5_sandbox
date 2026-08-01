#pragma once

#include "CoreMinimal.h"


struct SANDBOXCORE_API FGenerationIndex {

    static constexpr int32 INDEX_NONE{-1};

    FGenerationIndex() = default;
    FGenerationIndex(int32 index, int32 generation)
        : index(index)
        , generation(generation) {}

    bool operator==(FGenerationIndex const&) const noexcept = default;

    [[nodiscard]] auto is_valid() const noexcept -> bool;
    auto to_string() const -> FString;

    int32 index{INDEX_NONE};

    int32 generation{INDEX_NONE};
};
