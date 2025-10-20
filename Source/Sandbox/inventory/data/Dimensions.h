#pragma once

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Dimensions.generated.h"

USTRUCT(BlueprintType)
struct FDimensions {
    GENERATED_BODY()
  private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    FIntPoint value{};
  public:
    explicit FDimensions(FIntPoint v = {})
        : value(v) {}
    template <typename... Args>
        requires std::constructible_from<FIntPoint, Args...>
    explicit FDimensions(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    explicit operator FIntPoint() const { return value; }

    FIntPoint get_value() const { return value; }

    auto area() const { return value.X * value.Y; }
    template <typename Self>
    auto&& x(this Self&& self) {
        return std::forward_like<Self>(self.value.X);
    }
    template <typename Self>
    auto&& y(this Self&& self) {
        return std::forward_like<Self>(self.value.Y);
    }

    FString to_string() const { return FString::Printf(TEXT("x=%d, y=%d"), x(), y()); }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FDimensions const& obj) { return GetTypeHash(obj.value); }
};
