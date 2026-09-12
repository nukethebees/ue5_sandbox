#include "SpaceGameSimulation/memory/GameMemoryBacking.h"

auto FGameMemoryBacking::create(SIZE_T const capacity_bytes) -> TUniquePtr<FGameMemoryBacking> {
    check(capacity_bytes > 0);
    return TUniquePtr<FGameMemoryBacking>{new FGameMemoryBacking{capacity_bytes}};
}

FGameMemoryBacking::FGameMemoryBacking(SIZE_T const capacity_bytes)
    : backing_{ml::memory::Backing::create(capacity_bytes)} {}

FGameMemoryBacking::~FGameMemoryBacking() = default;

auto FGameMemoryBacking::try_acquire_lease() -> TOptional<FGameMemoryBackingLease> {
    auto lease{backing_->try_acquire_lease()};
    if (!lease.has_value()) {
        return NullOpt;
    }

    return FGameMemoryBackingLease{*this, std::move(*lease)};
}
