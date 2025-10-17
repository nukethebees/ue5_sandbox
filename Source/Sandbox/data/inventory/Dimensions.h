#pragma once

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Dimensions.generated.h"

USTRUCT(BlueprintType)
struct FDimensions {
    GENERATED_BODY()
  private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
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

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FDimensions const& obj) { return GetTypeHash(obj.value); }
};
