#include <SandboxCore/mimalloc_storage_allocator.h>
#include <SandboxCore/single_allocation/runtime.h>
#include <sbx/memory.h>

namespace ml::soa_storage {
auto MimallocStorageAllocator::allocate(SIZE_T const bytes, uint32 const alignment) -> std::byte* {
    auto* const allocation{sbx::memory::allocate_aligned(bytes, alignment)};
    require(allocation != nullptr);
    return ::new (allocation) std::byte[bytes];
}
void MimallocStorageAllocator::free(std::byte* data) noexcept {
    if (data != nullptr) {
        sbx::memory::free(data);
    }
}
auto MimallocStorageAllocator::reallocate(void* data, SIZE_T const bytes, uint32 const alignment)
    -> void* {
    auto* const allocation{sbx::memory::reallocate_aligned(data, bytes, alignment)};
    require(allocation != nullptr);
    return allocation;
}
auto MimallocStorageAllocator::owns(void const* data) noexcept -> bool {
    return sbx::memory::owns(data);
}
}
