#pragma once

#include "CoreMinimal.h"

#include <cstring>
#include <malloc.h>

struct FMemory {
    static auto Malloc(SIZE_T const bytes, uint32 const alignment) -> void* {
#ifdef _MSC_VER
        return _aligned_malloc(bytes, alignment);
#else
        return std::aligned_alloc(alignment, (bytes + alignment - 1) / alignment * alignment);
#endif
    }
    static void Free(void* const pointer) {
#ifdef _MSC_VER
        _aligned_free(pointer);
#else
        std::free(pointer);
#endif
    }
    static void Memcpy(void* const dst, void const* const src, SIZE_T const bytes) {
        std::memcpy(dst, src, bytes);
    }
};
