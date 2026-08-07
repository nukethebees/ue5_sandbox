#include <SandboxCoreEngine/uobject_utils.h>

#include <SandboxCore/error_msg.h>
#include <SandboxCore/log_categories.h>

#include "Containers/UnrealString.h"
#include "UObject/UObjectGlobals.h"

namespace ml {
void fatal_if_uobject_ptrs_invalid(std::initializer_list<NamedUObjectPtr> ptrs) {
    FErrorMsg error_msg;
    report_invalid_uobject_ptrs(ptrs, error_msg);

    if (!error_msg) {
        return;
    }

    UE_LOG(LogSandboxCore, Fatal, TEXT("UObject pointers are invalid:\n%s"), *error_msg.message);
}

auto report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs) -> FString {
    FErrorMsg error_msg;
    report_invalid_uobject_ptrs(ptrs, error_msg);
    return MoveTemp(error_msg.message);
}

bool report_invalid_uobject_ptrs(std::initializer_list<NamedUObjectPtr> ptrs,
                                 FErrorMsg& error_msg) {
    error_msg.message.Reset();

    for (auto const& ptr : ptrs) {
        if (!IsValid(ptr.object)) {
            error_msg.message += FString::Printf(TEXT("    %s\n"), ptr.name);
        }
    }

    return error_msg.has_error();
}

bool report_invalid_uobject_ptrs(
    std::initializer_list<std::initializer_list<NamedUObjectPtr>> ptr_batches,
    FErrorMsg& error_msg) {
    error_msg.message.Reset();

    for (auto const& ptr_batch : ptr_batches) {
        if (report_invalid_uobject_ptrs(ptr_batch, error_msg)) {
            return true;
        }
    }

    return error_msg.has_error();
}
}
