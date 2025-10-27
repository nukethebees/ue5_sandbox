#pragma once

#include "CoreMinimal.h"

#include "ScreenBounds.generated.h"

USTRUCT(BlueprintType)
struct FScreenBounds {
    GENERATED_BODY()

    FScreenBounds() = default;
    FScreenBounds(FVector2D const& origin, FVector2D const& size)
        : origin(origin)
        , size(size) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D origin; // Top-left corner
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D size; // Width and height

    auto get_centre() const -> FVector2D { return origin + size * 0.5f; }
    auto get_bottom_right() const -> FVector2D { return origin + size; }

    auto to_string() const -> FString {
        return FString::Printf(TEXT("Origin: %s, Size: %s"), *origin.ToString(), *size.ToString());
    }
};
