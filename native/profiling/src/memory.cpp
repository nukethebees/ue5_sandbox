#include "sandbox/profiling/memory.h"

#include <tracy/Tracy.hpp>

namespace ml {
namespace profiling {
namespace memory_profiling {
constexpr std::int32_t allocation_callstack_depth{16};

auto domain_name(MemoryDomain const domain) noexcept -> char const* {
    switch (domain) {
        case MemoryDomain::PersistentRoot:
            return "Simulation/Persistent Root Memory";
        case MemoryDomain::FrameScratch:
            return "Simulation/Frame Scratch";
        case MemoryDomain::NativeSoa:
            return "Simulation/Native SOA";
        case MemoryDomain::Mimalloc:
            return "Simulation/Mimalloc";
    }

    return "Simulation/Unknown Memory";
}
}

void record_memory_allocation(MemoryDomain const domain,
                              void const* const pointer,
                              std::size_t const bytes) noexcept {
    if (pointer != nullptr) {
        TracyAllocNS(pointer,
                     bytes,
                     memory_profiling::allocation_callstack_depth,
                     memory_profiling::domain_name(domain));
    }
}

void record_memory_free(MemoryDomain const domain, void const* const pointer) noexcept {
    if (pointer != nullptr) {
        TracyFreeN(pointer, memory_profiling::domain_name(domain));
    }
}
}
}
