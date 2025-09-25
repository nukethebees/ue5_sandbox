#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "InputTriggers.h"

namespace ml {
struct EnhancedInputMixin {
    template <typename Self>
    decltype(auto) make_input_binder(this Self&& self, UEnhancedInputComponent* eic) {
        return [eic, owner = &self](auto* action, ETriggerEvent state, auto pmf) -> void {
            if (action) {
                eic->BindAction(action, state, owner, pmf);
            } else {
                owner->log_warning(TEXT("Binding action pointer missing."));
            }
        };
    }
};
}
