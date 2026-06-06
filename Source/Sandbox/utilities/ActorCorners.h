#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

#include "ActorCorners.generated.h"

USTRUCT()
struct FActorCorners {
    GENERATED_BODY()

    using Corners = TStaticArray<FVector, 8>;

    FActorCorners() = default;
    FActorCorners(Corners&& corners)
        : data(std::move(corners)) {}

    auto to_string() const -> FString;

    Corners data;
};
