#include "SpaceGameSimulation/memory/GameMemoryBackingLease.h"

#include "SpaceGameSimulation/memory/GameMemoryBacking.h"

#include <utility>

FGameMemoryBackingLease::FGameMemoryBackingLease(FGameMemoryBacking& backing,
                                                 ml::memory::BackingLease lease) noexcept
    : backing_{&backing}
    , lease_{std::move(lease)} {}

FGameMemoryBackingLease::~FGameMemoryBackingLease() = default;

FGameMemoryBackingLease::FGameMemoryBackingLease(FGameMemoryBackingLease&& other) noexcept
    : backing_{std::exchange(other.backing_, nullptr)}
    , lease_{std::move(other.lease_)} {}

auto FGameMemoryBackingLease::operator=(FGameMemoryBackingLease&& other) noexcept
    -> FGameMemoryBackingLease& {
    if (this != &other) {
        backing_ = std::exchange(other.backing_, nullptr);
        lease_ = std::move(other.lease_);
    }
    return *this;
}

auto FGameMemoryBackingLease::get() noexcept -> FGameMemoryBacking& {
    check(backing_ != nullptr);
    return *backing_;
}

auto FGameMemoryBackingLease::get() const noexcept -> FGameMemoryBacking const& {
    check(backing_ != nullptr);
    return *backing_;
}
