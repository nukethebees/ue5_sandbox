#pragma once

 	#include "Containers/Array.h"

namespace ml {
template <typename T>
void destroy_components_array(TArray<T>& components) {
    for (auto& component : components) {
        if (!IsValid(component)) {
            continue;
        }

        component->Modify();
        component->DestroyComponent();
    }

    components.Reset();
};
}
