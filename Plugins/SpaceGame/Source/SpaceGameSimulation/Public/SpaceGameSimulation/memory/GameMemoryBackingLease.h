#pragma once

#include "CoreMinimal.h"

#include <native/memory/backing.h>

class FGameMemoryBacking;

class SPACEGAMESIMULATION_API FGameMemoryBackingLease {
  public:
    FGameMemoryBackingLease() = default;
    ~FGameMemoryBackingLease();
    FGameMemoryBackingLease(FGameMemoryBackingLease const&) = delete;
    auto operator=(FGameMemoryBackingLease const&) -> FGameMemoryBackingLease& = delete;
    FGameMemoryBackingLease(FGameMemoryBackingLease&& other) noexcept;
    auto operator=(FGameMemoryBackingLease&& other) noexcept -> FGameMemoryBackingLease&;

    auto get() noexcept -> FGameMemoryBacking&;
    auto get() const noexcept -> FGameMemoryBacking const&;
    explicit operator bool() const noexcept { return backing_ != nullptr; }
  private:
    friend class FGameMemoryBacking;

    FGameMemoryBackingLease(FGameMemoryBacking& backing, ml::memory::BackingLease lease) noexcept;

    FGameMemoryBacking* backing_{};
    ml::memory::BackingLease lease_{};
};
