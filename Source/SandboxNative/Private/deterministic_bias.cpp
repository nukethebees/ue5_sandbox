#include "SandboxNative/deterministic_bias.h"

#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
auto make_deterministic_biases(TConstArrayView<int32> const first,
                               TConstArrayView<int32> const second,
                               TArrayView<uint32> const integral_out,
                               TArrayView<float> const floating_out) noexcept -> void {
    auto const count{first.Num()};
    check(second.Num() == count);
    check(integral_out.Num() == count);
    check(floating_out.Num() == count);

    for (int32 i{0}; i < count; ++i) {
        auto const biases{make_deterministic_biases(first[i], second[i])};
        integral_out[i] = biases.integral;
        floating_out[i] = biases.floating;
    }
}

auto make_deterministic_biases(TConstArrayView<FRegistryEntityHandle> const handles,
                               TArrayView<uint32> const integral_out) noexcept -> void {
    auto const count{handles.Num()};
    check(integral_out.Num() == count);

    for (int32 i{0}; i < count; ++i) {
        auto const handle{handles[i]};
        integral_out[i] = make_deterministic_integral_bias(handle.index, handle.generation);
    }
}
}
