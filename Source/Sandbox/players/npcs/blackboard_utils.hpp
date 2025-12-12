#pragma once

#include "CoreMinimal.h"

#include "BehaviorTree/BlackboardComponent.h"

namespace ml {
bool is_valid_key(UBlackboardComponent& bb, FName const& name);

template <typename T>
void set_bb_value(UBlackboardComponent& bb, FName const& name, T value) {
    check(is_valid_key(bb, name));

    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, int>) {
        bb.SetValueAsInt(name, value);
    } else if constexpr (std::is_same_v<U, float>) {
        bb.SetValueAsFloat(name, value);
    } else {
        static_assert(!sizeof(T), "Unhandled type");
    }
}
}
