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

    auto const succeeded{make_deterministic_biases(
        std::span{first.GetData(), static_cast<std::size_t>(first.Num())},
        std::span{second.GetData(), static_cast<std::size_t>(second.Num())},
        std::span{integral_out.GetData(), static_cast<std::size_t>(integral_out.Num())},
        std::span{floating_out.GetData(), static_cast<std::size_t>(floating_out.Num())})};
    check(succeeded);
}

auto make_deterministic_biases(TConstArrayView<FRegistryEntityHandle> const handles,
                               TArrayView<uint32> const integral_out) noexcept -> void {
    auto const count{handles.Num()};
    check(integral_out.Num() == count);

    auto const succeeded{make_deterministic_biases(
        std::span{handles.GetData(), static_cast<std::size_t>(handles.Num())},
        std::span{integral_out.GetData(), static_cast<std::size_t>(integral_out.Num())})};
    check(succeeded);
}
}
