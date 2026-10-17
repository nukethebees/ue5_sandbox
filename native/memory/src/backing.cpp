#include <native/memory/backing.h>

#include <cassert>
#include <new>
#include <stdexcept>

#if defined(IOJ_WITH_TRACY)
#include <sandbox/profiling/memory.h>
#endif

namespace ioj::memory {
namespace {
auto allocate_backing(std::size_t const capacity_bytes, std::pmr::memory_resource& upstream)
    -> std::byte* {
    if (capacity_bytes == 0) {
        throw std::invalid_argument{"Backing capacity must be greater than zero"};
    }

    auto* const allocation{
        static_cast<std::byte*>(upstream.allocate(capacity_bytes, Backing::alignment))};
#if defined(IOJ_WITH_TRACY)
    // Attribute retained backing to its physical allocation, not each borrower.
    if (&upstream == std::pmr::new_delete_resource()) {
        ml::profiling::record_memory_allocation(
            ml::profiling::MemoryDomain::PersistentRoot, allocation, capacity_bytes);
    }
#endif
    return allocation;
}
}

Backing::Backing(std::size_t const capacity_bytes, std::pmr::memory_resource& upstream)
    : upstream_{upstream}
    , data_{allocate_backing(capacity_bytes, upstream)}
    , capacity_bytes_{capacity_bytes} {}

Backing::~Backing() {
#if defined(IOJ_WITH_TRACY)
    if (&upstream_ == std::pmr::new_delete_resource()) {
        ml::profiling::record_memory_free(ml::profiling::MemoryDomain::PersistentRoot, data_);
    }
#endif
    upstream_.deallocate(data_, capacity_bytes_, alignment);
}
}
