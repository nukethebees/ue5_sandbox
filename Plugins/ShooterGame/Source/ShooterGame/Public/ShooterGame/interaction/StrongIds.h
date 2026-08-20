#pragma once

#include <compare>

#include "CoreMinimal.h"

struct ActorId {
  public:
    ActorId() = default;
    explicit ActorId(uint64 value)
        : value_{value} {}

    uint64 get() const { return value_; }

    auto operator<=>(ActorId const& other) const = default;
    bool operator==(ActorId const& other) const = default;

    ActorId& operator++() {
        ++value_;
        return *this;
    }

    ActorId operator++(int) {
        ActorId temp{*this};
        ++value_;
        return temp;
    }
  private:
    uint64 value_{0};
};

struct CombinedTriggerableId {
  public:
    CombinedTriggerableId() = default;
    explicit CombinedTriggerableId(uint64 value)
        : value_{value} {}

    uint64 get() const { return value_; }

    auto operator<=>(CombinedTriggerableId const& other) const = default;
    bool operator==(CombinedTriggerableId const& other) const = default;
  private:
    uint64 value_{0};
};

FORCEINLINE uint32 GetTypeHash(ActorId const& id) {
    return GetTypeHash(id.get());
}

FORCEINLINE uint32 GetTypeHash(CombinedTriggerableId const& id) {
    uint64 const value{id.get()};
    uint32 const low{static_cast<uint32>(value)};
    uint32 const high{static_cast<uint32>(value >> 32)};
    return HashCombine(GetTypeHash(low), GetTypeHash(high));
}
