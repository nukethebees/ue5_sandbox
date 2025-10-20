#pragma once

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Coord.generated.h"

USTRUCT(BlueprintType)
struct FCoord {
    GENERATED_BODY()
  private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    FIntPoint value{};
  public:
    explicit FCoord(FIntPoint v = {})
        : value(v) {}
    template <typename... Args>
        requires std::constructible_from<FIntPoint, Args...>
    explicit FCoord(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    explicit operator FIntPoint() const { return value; }

    FIntPoint get_value() const { return value; }

    template <typename Self>
    auto&& x(this Self&& self) {
        return std::forward_like<Self>(self.value.X);
    }
    template <typename Self>
    auto&& y(this Self&& self) {
        return std::forward_like<Self>(self.value.Y);
    }
    template <typename Self>
    auto&& row(this Self&& self) {
        return std::forward_like<Self>(self.value.X);
    }
    template <typename Self>
    auto&& column(this Self&& self) {
        return std::forward_like<Self>(self.value.Y);
    }

    FString to_string() const { return FString::Printf(TEXT("x=%d, y=%d"), x(), y()); }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FCoord const& obj) { return GetTypeHash(obj.value); }
};
