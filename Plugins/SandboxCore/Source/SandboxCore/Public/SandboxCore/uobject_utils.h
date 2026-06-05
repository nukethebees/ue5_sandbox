#pragma once

#include "Containers/UnrealString.h"

#include <initializer_list>

class UObject;

namespace ml {
struct SANDBOXCORE_API NamedUObjectPtr {
    UObject const* object{nullptr};
    TCHAR const* name{nullptr};
};

SANDBOXCORE_API void fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs);

#define SANDBOX_NAMED_UOBJECT_PTR(ptr) \
    ml::NamedUObjectPtr {              \
        ptr, TEXT(#ptr)                \
    }
}
