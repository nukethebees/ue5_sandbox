#pragma once

#include "CoreMinimal.h"

#include <memory>

template <typename T>
void DefaultConstructItems(void* const address, int32 const count) {
    auto* const elements{static_cast<T*>(address)};
    for (int32 index{}; index < count; ++index) {
        std::construct_at(elements + index);
    }
}
