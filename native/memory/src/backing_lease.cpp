#include <native/memory/backing_lease.h>

#include <native/memory/backing.h>

#include <cassert>
#include <utility>

namespace ioj::memory {
BackingLease::BackingLease(Backing& backing) noexcept
    : backing_{&backing} {}

BackingLease::~BackingLease() {
    reset();
}

BackingLease::BackingLease(BackingLease&& other) noexcept
    : backing_{std::exchange(other.backing_, nullptr)} {}

auto BackingLease::operator=(BackingLease&& other) noexcept -> BackingLease& {
    if (this != &other) {
        reset();
        backing_ = std::exchange(other.backing_, nullptr);
    }
    return *this;
}

auto BackingLease::get() noexcept -> Backing& {
    assert(backing_ != nullptr);
    return *backing_;
}

auto BackingLease::get() const noexcept -> Backing const& {
    assert(backing_ != nullptr);
    return *backing_;
}

void BackingLease::reset() noexcept {
    if (backing_ != nullptr) {
        backing_->release_lease();
        backing_ = nullptr;
    }
}

}
