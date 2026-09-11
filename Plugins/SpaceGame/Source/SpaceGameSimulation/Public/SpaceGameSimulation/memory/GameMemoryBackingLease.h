#pragma once

#include "CoreMinimal.h"

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

    explicit FGameMemoryBackingLease(FGameMemoryBacking& backing) noexcept;
    void reset();

    FGameMemoryBacking* backing_{};
};
