#include <sbx/memory.h>

#include "sbx_mimalloc_prefix.h"

#include <mimalloc.h>

namespace sbx::memory {
auto allocate_aligned(std::size_t const bytes, std::size_t const alignment) noexcept -> void* {
    return mi_malloc_aligned(bytes, alignment);
}
auto reallocate_aligned(void* const data,
                        std::size_t const bytes,
                        std::size_t const alignment) noexcept -> void* {
    return mi_realloc_aligned(data, bytes, alignment);
}
void free(void* const data) noexcept {
    mi_free(data);
}
auto owns(void const* const data) noexcept -> bool {
    return mi_is_in_heap_region(data);
}
auto version() noexcept -> int {
    return mi_version();
}
}
