#include "sandbox/core/sandbox_ismc_transform.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <numbers>
#include <print>
#include <random>

namespace ml::sandbox_ismc {
namespace {
auto angular_error(Quaternion4f original, Quaternion4f decoded) -> double {
    // Double precision avoids acos amplification of float dot-product rounding.
    double dot{};
    double first_length{};
    double second_length{};
    for (auto index{0U}; index < 4; ++index) {
        auto const a{static_cast<double>(original.Elements[index])};
        auto const b{static_cast<double>(decoded.Elements[index])};
        dot += a * b;
        first_length += a * a;
        second_length += b * b;
    }
    return 2.0 *
           std::acos(
               std::clamp(std::abs(dot) / std::sqrt(first_length * second_length), 0.0, 1.0)) *
           180.0 / std::numbers::pi;
}

TEST(SandboxISMCPacking, GoldenQuaternionsAndSignEquivalence) {
    for (auto index{0U}; index < 4; ++index) {
        auto quaternion{HMM_Q(0, 0, 0, 0)};
        quaternion.Elements[index] = 1.0f;
        auto const packed{pack_quat32(quaternion)};
        ASSERT_TRUE(packed.has_value());
        EXPECT_EQ(packed->bits, 0x80200800U | index);
        EXPECT_EQ(pack_quat32(HMM_MulQF(quaternion, -1.0f))->bits, packed->bits);
        EXPECT_LT(angular_error(quaternion, unpack_quat32(*packed)), 0.15);
    }
    auto const boundary{
        pack_quat32(HMM_Q(quaternion_component_limit, -quaternion_component_limit, 0, 0))};
    ASSERT_TRUE(boundary.has_value());
    EXPECT_EQ(boundary->bits, 0x80200000U);
    EXPECT_EQ(pack_quat32(HMM_Q(0.5f, 0.5f, 0.5f, 0.5f))->bits, 0xda769da4U);
}

TEST(SandboxISMCPacking, AxesNearHalfTurnsAndArbitraryRotations) {
    for (auto axis{0U}; axis < 3; ++axis) {
        for (auto const angle : {0.0f, 0.001f, 1.0f, 1.5707963f, 3.14158f, 3.1415927f, 3.14160f}) {
            auto quaternion{HMM_Q(0, 0, 0, std::cos(angle * 0.5f))};
            quaternion.Elements[axis] = std::sin(angle * 0.5f);
            auto const packed{pack_quat32(quaternion)};
            ASSERT_TRUE(packed.has_value());
            EXPECT_LT(angular_error(quaternion, unpack_quat32(*packed)), 0.3);
        }
    }
    EXPECT_FALSE(pack_quat32(HMM_Q(0, 0, 0, 0)).has_value());
    EXPECT_FALSE(pack_quat32(HMM_Q(std::numeric_limits<float>::infinity(), 0, 0, 1)).has_value());
    EXPECT_FALSE(pack_quat32(HMM_Q(0, 0, 0, std::numeric_limits<float>::quiet_NaN())).has_value());
    auto const nonunit{HMM_Q(1, -2, 3, -4)};
    EXPECT_LT(angular_error(nonunit, unpack_quat32(*pack_quat32(nonunit))), 0.3);
}

TEST(SandboxISMCPacking, RandomQuaternionRoundTrips) {
    std::mt19937 random{0x513a2U};
    std::normal_distribution<float> normal{0.0f, 1.0f};
    double maximum{};
    double squared_error{};
    constexpr auto count{100000};
    for (auto index{0}; index < count; ++index) {
        auto const quaternion{
            HMM_NormQ(HMM_Q(normal(random), normal(random), normal(random), normal(random)))};
        auto const packed{pack_normalized_quat32(quaternion)};
        EXPECT_EQ(pack_normalized_quat32(HMM_MulQF(quaternion, -1.0f)).bits, packed.bits);
        auto const error{angular_error(quaternion, unpack_quat32(packed))};
        maximum = std::max(maximum, error);
        squared_error += error * error;
    }
    std::println("Quat32: {} samples; maximum {:.9f} degrees; RMS {:.9f} degrees",
                 count,
                 maximum,
                 std::sqrt(squared_error / count));
    EXPECT_LT(maximum, 0.3);
    EXPECT_LT(2.0 * std::sin(maximum * std::numbers::pi / 360.0), rotation_error_chord);
}

TEST(SandboxISMCPacking, PositionRangeAndRounding) {
    auto const quantize_units{[](float value, float root) {
        return quantize_position(value * position_quantum, root * position_quantum);
    }};
    EXPECT_EQ(quantize_units(1000, 1000), 0);
    EXPECT_EQ(quantize_units(1000.5f, 1000), 1);
    EXPECT_EQ(quantize_units(999.5f, 1000), 0);
    EXPECT_EQ(quantize_units(998.5f, 1000), -1);
    EXPECT_EQ(quantize_units(32767, 0), 32767);
    EXPECT_EQ(quantize_units(-32767, 0), -32767);
    EXPECT_FALSE(quantize_units(32767.5f, 0).has_value());
    EXPECT_FALSE(quantize_units(-32767.501f, 0).has_value());
    EXPECT_FALSE(quantize_units(std::numeric_limits<float>::quiet_NaN(), 0).has_value());
    for (auto const root : {-262144.0f, 262144.0f}) {
        for (auto const position :
             {std::nextafter(8.0f, 0.0f), 8.0f, std::nextafter(-8.0f, -9.0f), -8.0f}) {
            auto const expected{std::floor((static_cast<double>(position) - root) / 16.0 + 0.5)};
            EXPECT_EQ(quantize_position(position, root), static_cast<std::int16_t>(expected));
        }
    }
    std::mt19937 random{17};
    std::uniform_real_distribution<float> values{-32767, 32767};
    for (auto index{0}; index < 10000; ++index) {
        auto const value{values(random)};
        auto const packed{quantize_units(value, 0)};
        ASSERT_TRUE(packed.has_value());
        EXPECT_LE(std::abs(value - static_cast<float>(*packed)), 0.5f);
    }
}

TEST(SandboxISMCPacking, NormalizedEncoderContractAndComponentEdges) {
    EXPECT_TRUE(is_normalized_quaternion(HMM_Q(0, 0, 0, 1)));
    EXPECT_FALSE(is_normalized_quaternion(HMM_Q(0, 0, 0, 0)));
    EXPECT_FALSE(is_normalized_quaternion(HMM_Q(0, 0, 0, 1.001f)));
    EXPECT_FALSE(is_normalized_quaternion(HMM_Q(0, 0, 0, std::numeric_limits<float>::quiet_NaN())));
    EXPECT_FALSE(is_normalized_quaternion(HMM_Q(0, 0, 0, std::numeric_limits<float>::infinity())));
    for (auto const direction : {-1.0f, 0.0f, 1.0f}) {
        EXPECT_EQ(
            quantize_quaternion_component(std::nextafter(-quaternion_component_limit, direction)),
            0U);
        EXPECT_EQ(
            quantize_quaternion_component(std::nextafter(quaternion_component_limit, direction)),
            1023U);
    }
    EXPECT_EQ(pack_normalized_quat32(HMM_Q(0, 0, 0, 1)).bits, 0x80200803U);
    EXPECT_EQ(pack_normalized_quat32(HMM_Q(0.5f, 0.5f, 0.5f, 0.5f)).bits, 0xda769da4U);
}

TEST(SandboxISMCPacking, PositionDomainChoosesAnAlignedCentreAndRejectsExcessRange) {
    auto const root{position_root(make_vector3f(-480000, -1608, 1600000),
                                  make_vector3f(480000, 1624, 1600320))};
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->X, 0);
    EXPECT_EQ(root->Y, 16);
    EXPECT_EQ(root->Z, 1600160);
    EXPECT_FALSE(
        position_root(make_vector3f(-524288, 0, 0), make_vector3f(524288, 0, 0)).has_value());
    EXPECT_FALSE(position_root(make_vector3f(1, 0, 0), make_vector3f(0, 0, 0)).has_value());
    EXPECT_FALSE(position_root(make_vector3f(0, 0, 0),
                               make_vector3f(std::numeric_limits<float>::infinity(), 0, 0))
                     .has_value());
}

TEST(SandboxISMCPacking, LittleEndianGpuWords) {
    PackedTransform packed{};
    packed.position[0] = -1;
    packed.position[1] = 32767;
    packed.position[2] = -32767;
    packed.rotation.bits = 0x80200803;
    EXPECT_EQ(packed.reserved, 0);
    auto const words{std::bit_cast<std::array<std::uint32_t, 3>>(packed)};
    EXPECT_EQ(words, (std::array<std::uint32_t, 3>{0x7fffffff, 0x00008001, 0x80200803}));
}
}
}
