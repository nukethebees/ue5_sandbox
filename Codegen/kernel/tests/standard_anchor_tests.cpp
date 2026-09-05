#include "standard/array_math_kernels.h"
#include "standard/candidate_math_kernels.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

TEST(KernelStandardAnchors, PreservesNonCommutativeOperandOrder) {
    std::array<std::int32_t, 3> values{12, 18, 24};

    ml::subtract_in_place(values, 2);
    ml::divide_in_place(values, 2);

    EXPECT_EQ(values, (std::array<std::int32_t, 3>{5, 8, 11}));
}

TEST(KernelStandardAnchors, LerpsAndExtrapolates) {
    std::array<float, 3> from{0.0f, 10.0f, -4.0f};
    std::array<float, 3> to{8.0f, 2.0f, 4.0f};
    std::array<float, 3> alpha{0.0f, 1.0f, 1.5f};
    std::array<float, 3> out{};

    ml::kernel_lab::lerp(from, to, alpha, out);

    EXPECT_EQ(out, (std::array<float, 3>{0.0f, 2.0f, 8.0f}));
}

TEST(KernelStandardAnchors, ComputesScaledArithmetic) {
    std::array<double, 3> base{1.0, 2.0, 3.0};
    std::array<double, 3> value{4.0, 5.0, 6.0};
    std::array<double, 3> out{};

    ml::kernel_lab::add_scaled(base, value, 0.5, out);
    EXPECT_EQ(out, (std::array<double, 3>{3.0, 4.5, 6.0}));

    ml::kernel_lab::subtract_scaled_in_place(base, value, 0.5);
    EXPECT_EQ(base, (std::array<double, 3>{-1.0, -0.5, 0.0}));
}

TEST(KernelStandardAnchors, SupportsFixedWidthUnsignedTypes) {
    std::array<std::uint32_t, 3> values{2, 3, 4};
    std::array<std::uint32_t, 3> out{};

    ml::kernel_lab::square(values, out);

    EXPECT_EQ(out, (std::array<std::uint32_t, 3>{4, 9, 16}));
}

TEST(KernelStandardAnchors, ComputesVectorComponentExpressions) {
    std::array<float, 2> ax{1.0f, 2.0f};
    std::array<float, 2> ay{2.0f, 3.0f};
    std::array<float, 2> az{3.0f, 4.0f};
    std::array<float, 2> bx{4.0f, 5.0f};
    std::array<float, 2> by{5.0f, 6.0f};
    std::array<float, 2> bz{6.0f, 7.0f};
    std::array<float, 2> out{};

    ml::kernel_lab::dot_product_3d(ax, ay, az, bx, by, bz, out);
    EXPECT_EQ(out, (std::array<float, 2>{32.0f, 56.0f}));

    ml::kernel_lab::distance_squared_3d(ax, ay, az, bx, by, bz, out);
    EXPECT_EQ(out, (std::array<float, 2>{27.0f, 27.0f}));

    ml::kernel_lab::size_squared_3d(ax, ay, az, out);
    EXPECT_EQ(out, (std::array<float, 2>{14.0f, 29.0f}));
}

TEST(KernelStandardAnchors, PreservesFloatingPointGrouping) {
    std::array<float, 1> ax{1.0e20f};
    std::array<float, 1> ay{-1.0e20f};
    std::array<float, 1> az{3.0f};
    std::array<float, 1> unit{1.0f};
    std::array<float, 1> out{};

    ml::kernel_lab::dot_product_3d(ax, ay, az, unit, unit, unit, out);

    EXPECT_EQ(out[0], 3.0f);
}

TEST(KernelStandardInvariants, AcceptsAliasedReadOnlyInputs) {
    std::array<float, 2> input{2.0f, 4.0f};
    std::array<float, 2> out{};

    ml::kernel_lab::lerp(input, input, 0.5f, out);

    EXPECT_EQ(out, input);
}

TEST(KernelStandardInvariants, RejectsUnequalLengths) {
    EXPECT_DEATH_IF_SUPPORTED(
        ([] {
            std::array<float, 2> from{};
            std::array<float, 1> to{};
            std::array<float, 2> out{};
            ml::kernel_lab::lerp(from, to, 0.5f, out);
        }()),
        "");
}

TEST(KernelStandardInvariants, RejectsOverlappingOutput) {
    EXPECT_DEATH_IF_SUPPORTED(
        ([] {
            std::array<float, 2> storage{};
            std::array<float, 2> to{};
            ml::kernel_lab::lerp(storage, to, 0.5f, std::span<float>{storage});
        }()),
        "");
}

TEST(KernelStandardInvariants, RejectsOverlappingInPlaceInput) {
    EXPECT_DEATH_IF_SUPPORTED(
        ([] {
            std::array<float, 2> storage{};
            ml::kernel_lab::lerp_in_place(
                std::span<float>{storage}, std::span<float const>{storage}, 0.5f);
        }()),
        "");
}
