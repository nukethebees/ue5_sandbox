#pragma once

#include <utility>

#include "CoreMinimal.h"

struct FActorCorners {
    using Corners = TStaticArray<FVector, 8>;

    FActorCorners() = default;
    FActorCorners(Corners&& corners)
        : data(std::move(corners)) {}

    FString to_string() const {
        FString out{};

        for (auto const& corner : data) {
            out += FString::Printf(TEXT("%s\n"), *corner.ToString());
        }

        return out;
    }

    Corners data;
};
