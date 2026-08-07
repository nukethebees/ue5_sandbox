#pragma once

#include <SandboxCore/error_msg.h>

#include "Containers/UnrealString.h"

#include <initializer_list>

class UObject;

namespace ml {
struct SANDBOXCOREENGINE_API NamedUObjectPtr {
    UObject const* object{nullptr};
    TCHAR const* name{nullptr};
};

SANDBOXCOREENGINE_API auto report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs)
    -> FErrorMsg;
SANDBOXCOREENGINE_API bool report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs,
                                                       FErrorMsg& error_msg);

// Check multiple groups of pointers sequentially
// If one group fails, return and stop checking
// This is useful for dependent pointers e.g. IsValid(a) then IsValid(a->b)
SANDBOXCOREENGINE_API bool report_invalid_uobject_ptrs(
    std::initializer_list<std::initializer_list<NamedUObjectPtr>> ptr_batches,
    FErrorMsg& error_msg);

SANDBOXCOREENGINE_API void
    fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs);
SANDBOXCOREENGINE_API void fatal_if_uobject_ptrs_invalid(
    std::initializer_list<std::initializer_list<NamedUObjectPtr>> ptr_batches);

#define SANDBOX_NAMED_UOBJECT_PTR(ptr) \
    ml::NamedUObjectPtr {              \
        ptr, TEXT(#ptr)                \
    }
}
