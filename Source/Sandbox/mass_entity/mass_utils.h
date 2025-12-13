#pragma once

#include "MassEntityTypes.h"

namespace ml {
template <typename T, typename... Ts>
void add_fragments(auto& fragments) {
    fragments.Add(*T::StaticStruct());
    if constexpr (sizeof...(Ts)) {
        ml::add_fragments<Ts...>(fragments);
    }
}
}
