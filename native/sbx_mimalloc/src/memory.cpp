#include <sbx/memory.h>

#include "sbx_mimalloc_prefix.h"

#include <mimalloc.h>

#if defined(SANDBOX_WITH_TRACY)
#include <sandbox/profiling/memory.h>

#include <mutex>
#endif

namespace sbx::memory {
#if defined(SANDBOX_WITH_TRACY)
namespace profiling_detail {
std::mutex allocation_mutex;
}
#endif

auto allocate_aligned(std::size_t const bytes, std::size_t const alignment) noexcept -> void* {
#if defined(SANDBOX_WITH_TRACY)
    std::lock_guard const lock{profiling_detail::allocation_mutex};
#endif
    auto* const allocation{mi_malloc_aligned(bytes, alignment)};
#if defined(SANDBOX_WITH_TRACY)
    ml::profiling::record_memory_allocation(
        ml::profiling::MemoryDomain::Mimalloc, allocation, bytes);
#endif
    return allocation;
}
auto reallocate_aligned(void* const data,
                        std::size_t const bytes,
                        std::size_t const alignment) noexcept -> void* {
#if defined(SANDBOX_WITH_TRACY)
    std::lock_guard const lock{profiling_detail::allocation_mutex};
#endif
    auto* const allocation{mi_realloc_aligned(data, bytes, alignment)};
#if defined(SANDBOX_WITH_TRACY)
    if (allocation != nullptr) {
        ml::profiling::record_memory_free(ml::profiling::MemoryDomain::Mimalloc, data);
        ml::profiling::record_memory_allocation(
            ml::profiling::MemoryDomain::Mimalloc, allocation, bytes);
    }
#endif
    return allocation;
}
void free(void* const data) noexcept {
#if defined(SANDBOX_WITH_TRACY)
    std::lock_guard const lock{profiling_detail::allocation_mutex};
    ml::profiling::record_memory_free(ml::profiling::MemoryDomain::Mimalloc, data);
#endif
    mi_free(data);
}
auto owns(void const* const data) noexcept -> bool {
    return mi_is_in_heap_region(data);
}
auto version() noexcept -> int {
    return mi_version();
}
}
