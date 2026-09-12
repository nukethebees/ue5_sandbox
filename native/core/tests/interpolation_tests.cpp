#include <sandbox/core/interpolation.h>

#include <gtest/gtest.h>

#include <array>

TEST(NativeCoreInterpolation, SupportsScalarAndPerElementAlpha) {
    std::array<float, 3> const from{0.0f, 10.0f, -4.0f};
    std::array<float, 3> const to{10.0f, 20.0f, 4.0f};
    std::array<float, 3> const alpha{0.0f, 0.5f, 1.0f};
    std::array<float, 3> out{};

    ml::native_kernel::lerp(out.data(), from.data(), to.data(), alpha.data(), 3);
    EXPECT_EQ(out, (std::array<float, 3>{0.0f, 15.0f, 4.0f}));

    ml::native_kernel::lerp(out.data(), from.data(), to.data(), 0.25f, 3);
    EXPECT_EQ(out, (std::array<float, 3>{2.5f, 12.5f, -2.0f}));
}

TEST(NativeCoreInterpolation, SupportsInPlaceAndEmptyRanges) {
    std::array<double, 2> current{0.0, 10.0};
    std::array<double, 2> const target{4.0, 2.0};

    ml::native_kernel::lerp_in_place(current.data(), target.data(), 0.5, 2);
    EXPECT_EQ(current, (std::array<double, 2>{2.0, 6.0}));
    ml::native_kernel::lerp_in_place(current.data(), target.data(), 0.5, 0);
    EXPECT_EQ(current, (std::array<double, 2>{2.0, 6.0}));
}
