#pragma once

#include "CoreMinimal.h"

namespace ml {

template <typename T>
struct VectorTraits;

template <>
struct VectorTraits<FVector> {
    using Element = double;
};

template <>
struct VectorTraits<FVector3f> {
    using Element = float;
};

template <>
struct VectorTraits<FVector2D> {
    using Element = double;
};

template <>
struct VectorTraits<FVector2f> {
    using Element = float;
};

template <typename T>
using VectorElementT = typename VectorTraits<T>::Element;
}
