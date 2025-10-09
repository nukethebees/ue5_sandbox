#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for FIntPoint
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Coord.generated.h"

USTRUCT(BlueprintType)
struct FCoord {
    GENERATED_BODY()
  private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    FIntPoint value{};

  public:
    explicit FCoord(FIntPoint v = {}) : value(v) {}
    template <typename... Args>
        requires std::constructible_from<FIntPoint, Args...>
    explicit FCoord(Args&&... args) : value(std::forward<Args>(args)...) {}

    explicit operator FIntPoint() const { return value; }

    FIntPoint get_value() const { return value; }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FCoord const& obj) {
        return GetTypeHash(obj.value);
    }

};
