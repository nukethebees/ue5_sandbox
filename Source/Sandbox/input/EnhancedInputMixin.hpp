#pragma once

#include <Sandbox/logging/SandboxLogCategories.h>

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "InputTriggers.h"

#include <utility>

namespace ml {
struct EnhancedInputMixin {
    template <typename Self>
    auto make_input_binder(this Self&& self, UEnhancedInputComponent* eic) -> decltype(auto) {
        return [eic, owner = &self](auto* action, ETriggerEvent state, auto pmf) -> void {
            if (action) {
                eic->BindAction(action, state, owner, pmf);
            } else {
                UE_LOG(LogSandbox, Warning, TEXT("Binding action pointer is nullptr."));
            }
        };
    }
};
}
