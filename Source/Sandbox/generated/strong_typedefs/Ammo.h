#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for float
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "Ammo.generated.h"

USTRUCT(BlueprintType)
struct FAmmo {
    GENERATED_BODY()
  private:
    UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    float value{};

  public:
    explicit FAmmo(float v = 0.0f) : value(v) {}
    template <typename... Args>
        requires std::constructible_from<float, Args...>
    explicit FAmmo(Args&&... args) : value(std::forward<Args>(args)...) {}

    explicit operator float() const { return value; }

    float get_value() const { return value; }

    // Comparison operators
    bool operator==(FAmmo const& rhs) const { return value == rhs.value; }
    bool operator!=(FAmmo const& rhs) const { return value != rhs.value; }
    bool operator<(FAmmo const& rhs) const { return value < rhs.value; }
    bool operator<=(FAmmo const& rhs) const { return value <= rhs.value; }
    bool operator>(FAmmo const& rhs) const { return value > rhs.value; }
    bool operator>=(FAmmo const& rhs) const { return value >= rhs.value; }

    // Arithmetic operators
    FAmmo operator+(FAmmo const& rhs) const { return FAmmo{value + rhs.value}; }
    FAmmo operator-(FAmmo const& rhs) const { return FAmmo{value - rhs.value}; }
    FAmmo operator*(FAmmo const& rhs) const { return FAmmo{value * rhs.value}; }
    FAmmo operator/(FAmmo const& rhs) const { return FAmmo{value / rhs.value}; }

    FAmmo& operator+=(FAmmo const& rhs) { value += rhs.value; return *this; }
    FAmmo& operator-=(FAmmo const& rhs) { value -= rhs.value; return *this; }
    FAmmo& operator*=(FAmmo const& rhs) { value *= rhs.value; return *this; }
    FAmmo& operator/=(FAmmo const& rhs) { value /= rhs.value; return *this; }

    FAmmo operator-() const { return FAmmo{-value}; }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FAmmo const& obj) {
        return GetTypeHash(obj.value);
    }

};
