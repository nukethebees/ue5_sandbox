#include "SpaceGameSimulation/memory/GameMemoryBackingLease.h"

#include "SpaceGameSimulation/memory/GameMemoryBacking.h"

#include <utility>

FGameMemoryBackingLease::FGameMemoryBackingLease(FGameMemoryBacking& backing) noexcept
    : backing_{&backing} {}

FGameMemoryBackingLease::~FGameMemoryBackingLease() {
    reset();
}

FGameMemoryBackingLease::FGameMemoryBackingLease(FGameMemoryBackingLease&& other) noexcept
    : backing_{std::exchange(other.backing_, nullptr)} {}

auto FGameMemoryBackingLease::operator=(FGameMemoryBackingLease&& other) noexcept
    -> FGameMemoryBackingLease& {
    if (this != &other) {
        reset();
        backing_ = std::exchange(other.backing_, nullptr);
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

void FGameMemoryBackingLease::reset() {
    if (backing_ != nullptr) {
        backing_->release_lease();
        backing_ = nullptr;
    }
}
