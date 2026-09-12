#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

#include <sandbox/simulation/deterministic_bias.h>

struct FRegistryEntityHandle;

namespace ml {
using FDeterministicBiases = DeterministicBiases;

auto SANDBOXNATIVE_API make_deterministic_biases(TConstArrayView<int32> const first,
                                                 TConstArrayView<int32> const second,
                                                 TArrayView<uint32> const integral_out,
                                                 TArrayView<float> const floating_out) noexcept
    -> void;

auto SANDBOXNATIVE_API
    make_deterministic_biases(TConstArrayView<FRegistryEntityHandle> const handles,
                              TArrayView<uint32> const integral_out) noexcept -> void;
}
