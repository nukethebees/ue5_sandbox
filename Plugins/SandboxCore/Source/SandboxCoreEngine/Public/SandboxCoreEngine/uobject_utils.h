#pragma once

#include "Containers/UnrealString.h"

#include <initializer_list>

class UObject;

namespace ml {
struct SANDBOXCOREENGINE_API NamedUObjectPtr {
    UObject const* object{nullptr};
    TCHAR const* name{nullptr};
};

SANDBOXCOREENGINE_API void
    fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs);
SANDBOXCOREENGINE_API auto report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs)
    -> FString;

#define SANDBOX_NAMED_UOBJECT_PTR(ptr) \
    ml::NamedUObjectPtr {              \
        ptr, TEXT(#ptr)                \
    }
}
