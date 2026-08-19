#include "SandboxNative/deterministic_bias.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <array>
#include <limits>
#include <utility>

TEST_CASE("SandboxNative.DeterministicBias.StableGeneration") {
    constexpr auto biases{ml::make_deterministic_biases(42, 7)};

    CHECK(biases.integral == 2'408'474'220u);
    CHECK(biases.floating == 0.85223466f);
    CHECK(ml::make_deterministic_biases(42, 7).integral == biases.integral);
    CHECK(ml::make_deterministic_biases(42, 7).floating == biases.floating);

    std::array const first_values{42, 43};
    std::array const second_values{7, 8};
    std::array<uint32, 2> integral_out;
    std::array<float, 2> floating_out;
    ml::make_deterministic_biases({first_values.data(), static_cast<int32>(first_values.size())},
                                  {second_values.data(), static_cast<int32>(second_values.size())},
                                  {integral_out.data(), static_cast<int32>(integral_out.size())},
                                  {floating_out.data(), static_cast<int32>(floating_out.size())});

    for (int32 i{0}; i < static_cast<int32>(first_values.size()); ++i) {
        auto const expected{ml::make_deterministic_biases(first_values[i], second_values[i])};
        CHECK(integral_out[i] == expected.integral);
        CHECK(floating_out[i] == expected.floating);
    }

    ml::make_deterministic_biases(TConstArrayView<int32>{}, TConstArrayView<int32>{}, TArrayView<uint32>{}, TArrayView<float>{});
}

TEST_CASE("SandboxNative.DeterministicBias.ValidFloatRange") {
    constexpr std::array cases{
        std::pair{0, 0},
        std::pair{1, 0},
        std::pair{0, 1},
        std::pair{-1, -1},
        std::pair{std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max()},
        std::pair{std::numeric_limits<int32>::max(), std::numeric_limits<int32>::min()},
    };

    for (auto const [first, second] : cases) {
        auto const biases{ml::make_deterministic_biases(first, second)};
        CHECK(biases.floating >= 0.f);
        CHECK(biases.floating < 1.f);
    }
}

TEST_CASE("SandboxNative.DeterministicBias.BothInputsAffectResults") {
    constexpr auto base{ml::make_deterministic_biases(42, 7)};
    constexpr auto changed_first{ml::make_deterministic_biases(43, 7)};
    constexpr auto changed_second{ml::make_deterministic_biases(42, 8)};

    CHECK(changed_first.integral != base.integral);
    CHECK(changed_first.floating != base.floating);
    CHECK(changed_second.integral != base.integral);
    CHECK(changed_second.floating != base.floating);
}
