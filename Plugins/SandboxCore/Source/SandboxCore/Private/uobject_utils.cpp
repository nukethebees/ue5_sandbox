#include <SandboxCore/uobject_utils.h>

#include <SandboxCore/log_categories.h>

#include "Containers/UnrealString.h"

namespace ml {
void fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs) {
    FString msg;

    for (auto const& ptr : ptrs) {
        if (!IsValid(ptr.object)) {
            msg += FString::Printf(TEXT("    %s\n"), ptr.name);
        }
    }
    auto const have_invalid_ptrs{!msg.IsEmpty()};

    if (!have_invalid_ptrs) {
        return;
    }

    UE_LOG(LogSandboxCore, Fatal, TEXT("UObject pointers are invalid:\n%s"), *msg);
}
}
