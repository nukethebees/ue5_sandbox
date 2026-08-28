#pragma once

#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"

namespace ml::ui::benchmark {
inline auto percentile(TArray<double> const& sorted_samples, double const fraction) -> double {
    check(!sorted_samples.IsEmpty());
    auto const index{FMath::Clamp(
        FMath::CeilToInt(fraction * sorted_samples.Num()) - 1, 0, sorted_samples.Num() - 1)};
    return sorted_samples[index];
}
}
