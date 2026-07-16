#include <SandboxCore/uobject_utils.h>

#include <SandboxCore/log_categories.h>

#include "Containers/UnrealString.h"

namespace ml {
void fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs) {
    FString msg{report_invalid_uobject_ptrs(ptrs)};

    if (msg.IsEmpty()) { return; }

    UE_LOG(LogSandboxCore, Fatal, TEXT("UObject pointers are invalid:\n%s"), *msg);
}

auto report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs) -> FString {
    FString msg;

    for (auto const& ptr : ptrs) {
        if (!IsValid(ptr.object)) { msg += FString::Printf(TEXT("    %s\n"), ptr.name); }
    }

    return msg;
}
}
