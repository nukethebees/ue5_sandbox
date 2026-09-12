#include <native/memory/backing.h>

#include <cassert>
#include <new>
#include <stdexcept>
#include <utility>

namespace ml::memory {
namespace {
auto allocate_backing(std::size_t const capacity_bytes) -> std::byte* {
    if (capacity_bytes == 0) {
        throw std::invalid_argument{"Backing capacity must be greater than zero"};
    }

    return static_cast<std::byte*>(
        ::operator new(capacity_bytes, std::align_val_t{Backing::alignment}));
}
}

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

auto Backing::create(std::size_t const capacity_bytes) -> std::unique_ptr<Backing> {
    return std::unique_ptr<Backing>{new Backing{capacity_bytes}};
}

Backing::Backing(std::size_t const capacity_bytes)
    : data_{allocate_backing(capacity_bytes)}
    , capacity_bytes_{capacity_bytes} {}

Backing::~Backing() {
    assert(!leased_);
    ::operator delete(data_, std::align_val_t{alignment});
}

auto Backing::try_acquire_lease() -> std::optional<BackingLease> {
    if (leased_) {
        return std::nullopt;
    }

    leased_ = true;
    return std::optional<BackingLease>{BackingLease{*this}};
}

void Backing::release_lease() noexcept {
    assert(leased_);
    leased_ = false;
}
}
