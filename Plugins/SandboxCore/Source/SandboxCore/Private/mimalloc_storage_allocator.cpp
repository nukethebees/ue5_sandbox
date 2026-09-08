#include <HAL/PlatformProcess.h>
#include <Misc/Paths.h>
#include <SandboxCore/mimalloc_storage_allocator.h>
#include <SandboxCore/single_allocation_storage.h>

#define MI_SHARED_LIB 1
#include <mimalloc.h>

namespace ml::soa_storage {
struct MimallocApi {
    decltype(&mi_malloc_aligned) allocate;
    decltype(&mi_realloc_aligned) reallocate;
    decltype(&mi_free) free;
    decltype(&mi_is_in_heap_region) owns;

    MimallocApi() {
        auto const path{FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("sbx-mimalloc.dll"))};
        // Keep the DLL loaded for process lifetime, including destruction of static owners.
        auto* const dll{FPlatformProcess::GetDllHandle(*path)};
        checkf(dll != nullptr, TEXT("Unable to load experimental mimalloc: %s"), *path);
        require(dll != nullptr);
        allocate = reinterpret_cast<decltype(allocate)>(
            FPlatformProcess::GetDllExport(dll, TEXT("mi_malloc_aligned")));
        reallocate = reinterpret_cast<decltype(reallocate)>(
            FPlatformProcess::GetDllExport(dll, TEXT("mi_realloc_aligned")));
        free =
            reinterpret_cast<decltype(free)>(FPlatformProcess::GetDllExport(dll, TEXT("mi_free")));
        owns = reinterpret_cast<decltype(owns)>(
            FPlatformProcess::GetDllExport(dll, TEXT("mi_is_in_heap_region")));
        require(allocate != nullptr && reallocate != nullptr && free != nullptr && owns != nullptr);
    }
    static auto get() -> MimallocApi const& {
        static MimallocApi const api;
        return api;
    }
};
auto MimallocStorageAllocator::allocate(SIZE_T const bytes, uint32 const alignment) -> std::byte* {
    auto* const allocation{MimallocApi::get().allocate(bytes, alignment)};
    require(allocation != nullptr);
    return ::new (allocation) std::byte[bytes];
}
void MimallocStorageAllocator::free(std::byte* data) noexcept {
    if (data != nullptr) {
        MimallocApi::get().free(data);
    }
}
auto MimallocStorageAllocator::reallocate(void* data, SIZE_T const bytes, uint32 const alignment)
    -> void* {
    auto* const allocation{MimallocApi::get().reallocate(data, bytes, alignment)};
    require(allocation != nullptr);
    return allocation;
}
auto MimallocStorageAllocator::owns(void const* data) noexcept -> bool {
    return MimallocApi::get().owns(data);
}
}
