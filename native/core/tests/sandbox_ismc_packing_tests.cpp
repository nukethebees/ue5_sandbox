#include "sandbox_ismc_packing_avx2.h"
#include "sandbox_ismc_packing_avx512.h"
#include "sandbox_ismc_packing_highway.h"

#include "sandbox/core/sandbox_ismc_packing.h"

#include <cpuinfo_x86.h>
#include <gtest/gtest.h>
#include <hwy/targets.h>

#include <array>
#include <cstring>
#include <random>
#include <vector>

namespace ml::sandbox_ismc::packing_tests {
struct Inputs {
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 4>> rotations;

    explicit Inputs(std::size_t count)
        : positions(count)
        , rotations(count) {
        std::mt19937 random{0x513a2U};
        std::uniform_real_distribution<float> offsets{-480000, 480000};
        std::normal_distribution<float> normal{0, 1};
        for (std::size_t index{}; index < count; ++index) {
            positions[index] = {
                262144 + offsets(random), -262144 + offsets(random), 1000000 + offsets(random)};
            auto const q{
                HMM_NormQ(HMM_Q(normal(random), normal(random), normal(random), normal(random)))};
            rotations[index] = {q.X, q.Y, q.Z, q.W};
            if (index % 11 == 0) {
                for (auto& component : rotations[index]) {
                    component *= index % 2 == 0 ? 1.00002f : 0.99998f;
                }
            }
        }
    }
    auto view() const -> TransformInput {
        return {std::as_bytes(std::span{positions}), std::as_bytes(std::span{rotations})};
    }
};

enum class Backend { Avx2, Avx512, Production, HighwayAvx512 };
auto supported(Backend backend) -> bool {
    switch (backend) {
        case Backend::Avx2:
            return cpu_features::GetX86Info().features.avx2 != 0;
        case Backend::Avx512:
            return experiment::supports_avx512();
        case Backend::Production:
            return true;
        case Backend::HighwayAvx512:
            return (hwy::SupportedTargets() & HWY_AVX3) != 0;
    }
    return false;
}

auto compare(Inputs const& inputs, PackingParameters const& parameters, Backend backend) -> void {
    auto const pack_transforms{backend == Backend::Production ? ml::sandbox_ismc::pack_transforms
                               : backend == Backend::HighwayAvx512
                                   ? experiment::highway_avx512::pack_transforms
                               : backend == Backend::Avx512 ? experiment::pack_transforms_avx512
                                                            : experiment::pack_transforms_avx2};
    auto const pack_positions{backend == Backend::Production ? ml::sandbox_ismc::pack_positions
                              : backend == Backend::HighwayAvx512
                                  ? experiment::highway_avx512::pack_positions
                              : backend == Backend::Avx512 ? experiment::pack_positions_avx512
                                                           : experiment::pack_positions_avx2};
    auto const pack_rotations{backend == Backend::Production ? ml::sandbox_ismc::pack_rotations
                              : backend == Backend::HighwayAvx512
                                  ? experiment::highway_avx512::pack_rotations
                              : backend == Backend::Avx512 ? experiment::pack_rotations_avx512
                                                           : experiment::pack_rotations_avx2};
    auto const count{inputs.positions.size()};
    std::vector<PackedTransform> scalar(count + 2);
    for (auto& packed : scalar) {
        packed = {{-1234, 2345, -3456}, 0xbeef, {0xabcdef01}};
    }
    auto simd{scalar};
    auto separate{scalar};
    auto const scalar_output{std::span{scalar}.subspan(1, count)};
    auto const simd_output{std::span{simd}.subspan(1, count)};
    auto const separate_output{std::span{separate}.subspan(1, count)};
    TransformBounds scalar_bounds{};
    TransformBounds simd_bounds{};
    pack_transforms_scalar(inputs.view(), parameters, scalar_output, &scalar_bounds);
    pack_transforms(inputs.view(), parameters, simd_output, &simd_bounds);
    ASSERT_EQ(std::memcmp(scalar.data(), simd.data(), scalar.size() * sizeof(PackedTransform)), 0);
    EXPECT_EQ(scalar_bounds.valid, count != 0);
    EXPECT_EQ(simd_bounds.valid, scalar_bounds.valid);
    for (auto axis{0U}; axis < 3; ++axis) {
        EXPECT_EQ(scalar_bounds.minimum.Elements[axis], simd_bounds.minimum.Elements[axis]);
        EXPECT_EQ(scalar_bounds.maximum.Elements[axis], simd_bounds.maximum.Elements[axis]);
    }
    pack_positions(inputs.view().positions, parameters.position_root, separate_output);
    pack_rotations(inputs.view().rotations, separate_output);
    EXPECT_EQ(std::memcmp(scalar.data(), separate.data(), scalar.size() * sizeof(PackedTransform)),
              0);
    pack_positions_scalar(inputs.view().positions, parameters.position_root, separate_output);
    pack_rotations_scalar(inputs.view().rotations, separate_output);
    EXPECT_EQ(std::memcmp(scalar.data(), separate.data(), scalar.size() * sizeof(PackedTransform)),
              0);
    pack_transforms(inputs.view(), parameters, simd_output, nullptr);
    EXPECT_EQ(std::memcmp(scalar.data(), simd.data(), scalar.size() * sizeof(PackedTransform)), 0);
    pack_transforms_scalar(inputs.view(), parameters, separate_output);
    EXPECT_EQ(std::memcmp(scalar.data(), separate.data(), scalar.size() * sizeof(PackedTransform)),
              0);
    for (std::size_t index{}; index < count; ++index) {
        EXPECT_EQ(scalar_output[index].reserved, 0xbeef);
        auto const& q{inputs.rotations[index]};
        EXPECT_EQ(scalar_output[index].rotation.bits,
                  pack_normalized_quat32(q[0], q[1], q[2], q[3]).bits);
        for (auto axis{0U}; axis < 3; ++axis) {
            EXPECT_EQ(scalar_output[index].position[axis],
                      quantize_position_unchecked(inputs.positions[index][axis],
                                                  parameters.position_root.Elements[axis]));
        }
    }

    // The adapter accepts object representations, with no alignment requirement.
    auto const view{inputs.view()};
    std::vector<std::byte> bytes(view.positions.size() + view.rotations.size() + 2);
    if (count != 0) {
        std::memcpy(bytes.data() + 1, view.positions.data(), view.positions.size());
        std::memcpy(
            bytes.data() + 2 + view.positions.size(), view.rotations.data(), view.rotations.size());
    }
    pack_transforms({std::span{bytes}.subspan(1, view.positions.size()),
                     std::span{bytes}.subspan(2 + view.positions.size(), view.rotations.size())},
                    parameters,
                    simd_output,
                    nullptr);
    EXPECT_EQ(std::memcmp(scalar.data(), simd.data(), scalar.size() * sizeof(PackedTransform)), 0);
}

class SandboxISMCBatchPackingVariants : public ::testing::TestWithParam<Backend> {};

TEST_P(SandboxISMCBatchPackingVariants, RandomBatchesAndEveryTailMatchExactly) {
    if (!supported(GetParam())) {
        GTEST_SKIP() << "Requested ISA unavailable";
    }
    PackingParameters const parameters{make_vector3f(262144, -262144, 1000000),
                                       make_vector3f(31, -57, 123),
                                       make_vector3f(100, 17, 300)};
    for (std::size_t count{}; count <= 48; ++count) {
        SCOPED_TRACE(count);
        compare(Inputs{count}, parameters, GetParam());
    }
    for (auto const count : {64U, 256U, 2000U, 4000U, 40003U, 100001U}) {
        SCOPED_TRACE(count);
        compare(Inputs{count}, parameters, GetParam());
    }
}

TEST_P(SandboxISMCBatchPackingVariants, BoundaryPositionsAndQuaternionTiesAndSigns) {
    if (!supported(GetParam())) {
        GTEST_SKIP() << "Requested ISA unavailable";
    }
    Inputs inputs{0};
    for (auto const root : {0.0f, -262144.0f, 262144.0f}) {
        for (auto const boundary : {-524280.0f, -24.0f, -8.0f, 8.0f, 24.0f, 524264.0f}) {
            for (auto const position :
                 {std::nextafter(boundary, -std::numeric_limits<float>::infinity()),
                  boundary,
                  std::nextafter(boundary, std::numeric_limits<float>::infinity())}) {
                auto const value{position + root};
                if (!can_quantize_position(value, root)) {
                    continue;
                }
                for (auto largest{0U}; largest < 4; ++largest) {
                    for (auto const sign : {-1.0f, 1.0f}) {
                        inputs.positions.push_back({value, value, value});
                        std::array<float, 4> rotation{};
                        rotation[largest] = sign;
                        inputs.rotations.push_back(rotation);
                        inputs.positions.push_back({value, value, value});
                        rotation = {0.5f * sign, -0.5f * sign, 0.5f * sign, -0.5f * sign};
                        inputs.rotations.push_back(rotation);
                        inputs.positions.push_back({value, value, value});
                        rotation = {};
                        rotation[largest] = quaternion_component_limit * sign;
                        rotation[(largest + 1) % 4] = -quaternion_component_limit * sign;
                        inputs.rotations.push_back(rotation);
                    }
                }
            }
        }
        compare(inputs, {make_vector3f(root, root, root), {}, make_vector3f(7, 3, 19)}, GetParam());
        inputs.positions.clear();
        inputs.rotations.clear();
    }
}

INSTANTIATE_TEST_SUITE_P(
    Isa,
    SandboxISMCBatchPackingVariants,
    ::testing::Values(Backend::Avx2, Backend::Avx512, Backend::Production, Backend::HighwayAvx512),
    [](auto const& info) {
        switch (info.param) {
            case Backend::Avx2:
                return "Avx2";
            case Backend::Avx512:
                return "Avx512";
            case Backend::Production:
                return "Production";
            case Backend::HighwayAvx512:
                return "HighwayAvx512";
        }
        return "Unknown";
    });

TEST(SandboxISMCBatchPacking, DirectBasisMatchesIndependentlyRotatedCorners) {
    Inputs inputs{1000};
    for (auto const origin : {make_vector3f(0, 0, 0), make_vector3f(-71, 203, 19)}) {
        for (auto const extent : {make_vector3f(0, 0, 0), make_vector3f(7, 23, 11)}) {
            PackingParameters const parameters{
                make_vector3f(262144, -262144, 1000000), origin, extent};
            auto const count{inputs.positions.size()};
            for (std::size_t index{}; index < count; ++index) {
                auto rotation{inputs.rotations[index]};
                // Also exercise the accepted normalization tolerance.
                auto const scale{index % 2 == 0 ? 1.00002f : 0.99998f};
                for (auto& component : rotation) {
                    component *= scale;
                }
                auto const& position{inputs.positions[index]};
                PackedTransform packed{};
                TransformBounds bounds{};
                pack_transforms_scalar({std::as_bytes(std::span{&position, 1}),
                                        std::as_bytes(std::span{&rotation, 1})},
                                       parameters,
                                       std::span{&packed, 1},
                                       &bounds);
                std::array<double, 3> low{1e30, 1e30, 1e30};
                std::array<double, 3> high{-1e30, -1e30, -1e30};
                for (auto corner{0U}; corner < 8; ++corner) {
                    std::array<double, 3> v{};
                    std::array<double, 3> t{};
                    for (auto axis{0U}; axis < 3; ++axis) {
                        v[axis] =
                            static_cast<double>(origin.Elements[axis]) +
                            ((corner & (1U << axis)) != 0 ? 1.0 : -1.0) * extent.Elements[axis];
                    }
                    for (auto axis{0U}; axis < 3; ++axis) {
                        auto const j{(axis + 1) % 3};
                        auto const k{(axis + 2) % 3};
                        t[axis] = 2.0 * (rotation[j] * v[k] - rotation[k] * v[j]);
                    }
                    for (auto axis{0U}; axis < 3; ++axis) {
                        auto const j{(axis + 1) % 3};
                        auto const k{(axis + 2) % 3};
                        auto const result{position[axis] + v[axis] + rotation[3] * t[axis] +
                                          rotation[j] * t[k] - rotation[k] * t[j]};
                        low[axis] = std::min(low[axis], result);
                        high[axis] = std::max(high[axis], result);
                    }
                }
                for (auto axis{0U}; axis < 3; ++axis) {
                    // Four sequential float additions at world-coordinate magnitude.
                    auto const tolerance{4.0 * std::numeric_limits<float>::epsilon() *
                                         (std::abs(static_cast<double>(position[axis])) + 1000.0)};
                    EXPECT_NEAR(bounds.minimum.Elements[axis], low[axis], tolerance);
                    EXPECT_NEAR(bounds.maximum.Elements[axis], high[axis], tolerance);
                }
            }
        }
    }
}
}
