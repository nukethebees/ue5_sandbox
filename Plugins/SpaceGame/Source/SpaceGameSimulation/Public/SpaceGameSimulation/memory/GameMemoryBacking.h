#pragma once

#include "SpaceGameSimulation/memory/GameMemoryBackingLease.h"

#include "CoreMinimal.h"

#include <native/memory/backing.h>

#include <cstddef>
#include <memory>

class SPACEGAMESIMULATION_API FGameMemoryBacking {
  public:
    static auto create(SIZE_T capacity_bytes) -> TUniquePtr<FGameMemoryBacking>;

    ~FGameMemoryBacking();
    FGameMemoryBacking(FGameMemoryBacking const&) = delete;
    FGameMemoryBacking(FGameMemoryBacking&&) = delete;
    auto operator=(FGameMemoryBacking const&) -> FGameMemoryBacking& = delete;
    auto operator=(FGameMemoryBacking&&) -> FGameMemoryBacking& = delete;

    auto try_acquire_lease() -> TOptional<FGameMemoryBackingLease>;
    auto data() const noexcept -> std::byte* { return backing_->data(); }
    auto capacity_bytes() const noexcept -> SIZE_T { return backing_->capacity_bytes(); }
    auto is_leased() const noexcept -> bool { return backing_->is_leased(); }
  private:
    friend class FGameMemoryBackingLease;

    explicit FGameMemoryBacking(SIZE_T capacity_bytes);
    std::unique_ptr<ml::memory::Backing> backing_{};
};
