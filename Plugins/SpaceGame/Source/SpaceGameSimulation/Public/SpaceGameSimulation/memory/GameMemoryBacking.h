#pragma once

#include "SpaceGameSimulation/memory/GameMemoryBackingLease.h"

#include "CoreMinimal.h"

#include <cstddef>

class SPACEGAMESIMULATION_API FGameMemoryBacking {
  public:
    static auto create(SIZE_T capacity_bytes) -> TUniquePtr<FGameMemoryBacking>;

    ~FGameMemoryBacking();
    FGameMemoryBacking(FGameMemoryBacking const&) = delete;
    FGameMemoryBacking(FGameMemoryBacking&&) = delete;
    auto operator=(FGameMemoryBacking const&) -> FGameMemoryBacking& = delete;
    auto operator=(FGameMemoryBacking&&) -> FGameMemoryBacking& = delete;

    auto try_acquire_lease() -> TOptional<FGameMemoryBackingLease>;
    auto data() const noexcept -> std::byte* { return data_; }
    auto capacity_bytes() const noexcept -> SIZE_T { return capacity_bytes_; }
    auto is_leased() const noexcept -> bool { return leased_; }
  private:
    friend class FGameMemoryBackingLease;

    explicit FGameMemoryBacking(SIZE_T capacity_bytes);
    void release_lease();

    std::byte* data_{};
    SIZE_T capacity_bytes_{};
    bool leased_{};
};
