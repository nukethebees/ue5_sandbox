#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

namespace ml {
template <auto Func, typename Container>
auto map_array(Container const& elems) {
    using T = decltype(*elems.begin());
    using U = decltype(Func(T{}));
    TArray<U> out;
    out.Reserve(elems.Num());

    for (auto const& elem : elems) {
        out.Add(Func(elem));
    }

    return out;
}

template <typename Fn, typename Container>
auto map_array(Container const& elems, Fn&& fn) {
    using T = decltype(*elems.begin());
    using U = decltype(fn(T{}));
    TArray<U> out;
    out.Reserve(elems.Num());

    for (auto const& elem : elems) {
        out.Add(fn(elem));
    }

    return out;
}
}
