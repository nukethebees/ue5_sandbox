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

void add_values(FMassArchetypeSharedFragmentValues& sv, auto& value, auto&... remaining) {
    sv.Add(value);
    if constexpr (sizeof...(remaining)) {
        add_values(sv, remaining...);
    } else {
        sv.Sort();
    }
}
}
