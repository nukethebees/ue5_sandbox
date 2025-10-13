#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for int32
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "BulletTypeIndex.generated.h"

USTRUCT(BlueprintType)
struct FBulletTypeIndex {
    GENERATED_BODY()
  private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    int32 value{};

  public:
    explicit FBulletTypeIndex(int32 v = 0) : value(v) {}
    template <typename... Args>
        requires std::constructible_from<int32, Args...>
    explicit FBulletTypeIndex(Args&&... args) : value(std::forward<Args>(args)...) {}

    explicit operator int32() const { return value; }

    int32 get_value() const { return value; }

    // Comparison operators
    bool operator==(FBulletTypeIndex const& rhs) const { return value == rhs.value; }
    bool operator!=(FBulletTypeIndex const& rhs) const { return value != rhs.value; }
    bool operator<(FBulletTypeIndex const& rhs) const { return value < rhs.value; }
    bool operator<=(FBulletTypeIndex const& rhs) const { return value <= rhs.value; }
    bool operator>(FBulletTypeIndex const& rhs) const { return value > rhs.value; }
    bool operator>=(FBulletTypeIndex const& rhs) const { return value >= rhs.value; }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FBulletTypeIndex const& obj) {
        return GetTypeHash(obj.value);
    }

};
