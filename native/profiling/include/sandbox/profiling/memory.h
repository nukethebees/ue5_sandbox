#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && defined(IOJ_TRACY_MEMORY_EXPORTS)
#define IOJ_TRACY_MEMORY_API __declspec(dllexport)
#else
#define IOJ_TRACY_MEMORY_API
#endif

namespace ml {
namespace profiling {
enum class MemoryDomain : std::uint8_t {
    PersistentRoot,
    FrameScratch,
    NativeSoa,
    Mimalloc,
};

extern "C" IOJ_TRACY_MEMORY_API void
    record_memory_allocation(MemoryDomain domain, void const* pointer, std::size_t bytes) noexcept;
extern "C" IOJ_TRACY_MEMORY_API void record_memory_free(MemoryDomain domain,
                                                        void const* pointer) noexcept;
}
}

#undef IOJ_TRACY_MEMORY_API
