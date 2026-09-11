#include "SpaceGameSimulation/memory/GameMemoryBacking.h"

#include "HAL/UnrealMemory.h"

auto FGameMemoryBacking::create(SIZE_T const capacity_bytes) -> TUniquePtr<FGameMemoryBacking> {
    check(capacity_bytes > 0);
    return TUniquePtr<FGameMemoryBacking>{new FGameMemoryBacking{capacity_bytes}};
}

FGameMemoryBacking::FGameMemoryBacking(SIZE_T const capacity_bytes)
    : data_{static_cast<std::byte*>(FMemory::Malloc(capacity_bytes, 64))}
    , capacity_bytes_{capacity_bytes} {
    checkf(data_ != nullptr,
           TEXT("Failed to allocate %llu bytes for the game-memory backing."),
           static_cast<uint64>(capacity_bytes));
}

FGameMemoryBacking::~FGameMemoryBacking() {
    checkf(!leased_, TEXT("Cannot destroy game-memory backing while it is leased."));
    FMemory::Free(data_);
}

auto FGameMemoryBacking::try_acquire_lease() -> TOptional<FGameMemoryBackingLease> {
    if (leased_) {
        return NullOpt;
    }

    leased_ = true;
    return FGameMemoryBackingLease{*this};
}

void FGameMemoryBacking::release_lease() {
    check(leased_);
    leased_ = false;
}
