#pragma once

/**
 * GENERATED CODE - DO NOT EDIT MANUALLY
 * Strong typedef wrapper for int32
 * Regenerate using the SandboxEditor 'Generate Typedefs' toolbar button
 */

#include <concepts>
#include <utility>

#include "CoreMinimal.h"

#include "StackSize.generated.h"

USTRUCT(BlueprintType)
struct FStackSize {
    GENERATED_BODY()
  private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    int32 value{};
  public:
    explicit FStackSize(int32 v = 0)
        : value(v) {}
    template <typename... Args>
        requires std::constructible_from<int32, Args...>
    explicit FStackSize(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    explicit operator int32() const { return value; }

    int32 get_value() const { return value; }

    // Comparison operators
    bool operator==(FStackSize const& rhs) const { return value == rhs.value; }
    bool operator!=(FStackSize const& rhs) const { return value != rhs.value; }
    bool operator<(FStackSize const& rhs) const { return value < rhs.value; }
    bool operator<=(FStackSize const& rhs) const { return value <= rhs.value; }
    bool operator>(FStackSize const& rhs) const { return value > rhs.value; }
    bool operator>=(FStackSize const& rhs) const { return value >= rhs.value; }

    // Arithmetic operators
    FStackSize operator+(FStackSize const& rhs) const { return FStackSize{value + rhs.value}; }
    FStackSize operator-(FStackSize const& rhs) const { return FStackSize{value - rhs.value}; }
    FStackSize operator*(FStackSize const& rhs) const { return FStackSize{value * rhs.value}; }
    FStackSize operator/(FStackSize const& rhs) const { return FStackSize{value / rhs.value}; }

    FStackSize& operator+=(FStackSize const& rhs) {
        value += rhs.value;
        return *this;
    }
    FStackSize& operator-=(FStackSize const& rhs) {
        value -= rhs.value;
        return *this;
    }
    FStackSize& operator*=(FStackSize const& rhs) {
        value *= rhs.value;
        return *this;
    }
    FStackSize& operator/=(FStackSize const& rhs) {
        value /= rhs.value;
        return *this;
    }

    FStackSize operator-() const { return FStackSize{-value}; }

    // Increment operators
    FStackSize& operator++() {
        ++value;
        return *this;
    }
    FStackSize operator++(int) { return FStackSize{value++}; }
    FStackSize& operator--() {
        --value;
        return *this;
    }
    FStackSize operator--(int) { return FStackSize{value--}; }

    // Modulo operators
    FStackSize operator%(FStackSize const& rhs) const { return FStackSize{value % rhs.value}; }

    FStackSize& operator%=(FStackSize const& rhs) {
        value %= rhs.value;
        return *this;
    }

    // Hash support for TMap/TSet
    friend uint32 GetTypeHash(FStackSize const& obj) { return GetTypeHash(obj.value); }
};
