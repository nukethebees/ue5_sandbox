#include <native/memory/backing.h>

#include <cassert>
#include <new>
#include <stdexcept>

#if defined(IOJ_WITH_TRACY)
#include <sandbox/profiling/memory.h>
#endif

namespace ml::memory {
namespace {
auto allocate_backing(std::size_t const capacity_bytes) -> std::byte* {
    if (capacity_bytes == 0) {
        throw std::invalid_argument{"Backing capacity must be greater than zero"};
    }

    auto* const allocation{static_cast<std::byte*>(
        ::operator new(capacity_bytes, std::align_val_t{Backing::alignment}))};
#if defined(IOJ_WITH_TRACY)
    profiling::record_memory_allocation(
        profiling::MemoryDomain::PersistentRoot, allocation, capacity_bytes);
#endif
    return allocation;
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
#if defined(IOJ_WITH_TRACY)
    profiling::record_memory_free(profiling::MemoryDomain::PersistentRoot, data_);
#endif
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
