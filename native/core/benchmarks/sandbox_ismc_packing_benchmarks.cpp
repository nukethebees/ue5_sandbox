#include "sandbox_ismc_packing_avx512.h"
#include "sandbox_ismc_packing_highway.h"

#include "sandbox/core/sandbox_ismc_packing.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>
#include <hwy/targets.h>

#include <array>
#include <random>
#include <string>
#include <vector>

namespace ml::sandbox_ismc::packing_benchmarks {
enum class Operation {
    Position,
    Quaternion,
    Transform,
    TransformBounds,
};

enum class Implementation {
    Scalar,
    Avx2,
    Avx512,
    HighwayAvx2,
    HighwayAvx512,
};

struct Buffers {
    explicit Buffers(std::size_t count)
        : positions(count)
        , rotations(count)
        , output(count) {
        std::mt19937 random{0x513a2U};
        std::uniform_real_distribution<float> offset{-480000, 480000};
        std::normal_distribution<float> normal{0, 1};
        for (std::size_t index{}; index < count; ++index) {
            positions[index] = {
                262144 + offset(random), -262144 + offset(random), 1000000 + offset(random)};
            auto const q{
                HMM_NormQ(HMM_Q(normal(random), normal(random), normal(random), normal(random)))};
            rotations[index] = {q.X, q.Y, q.Z, q.W};
        }
    }
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 4>> rotations;
    std::vector<PackedTransform> output;
};

template <Operation operation, Implementation implementation, bool Coherent = false>
auto run(benchmark::State& state) -> void {
    if constexpr (implementation == Implementation::Avx2) {
        if (cpu_features::GetX86Info().features.avx2 == 0) {
            state.SkipWithMessage("AVX2 unavailable");
            return;
        }
    }
    if constexpr (implementation == Implementation::Avx512) {
        if (!experiment::supports_avx512()) {
            state.SkipWithMessage("AVX512 unavailable");
            return;
        }
    }
    if constexpr (implementation == Implementation::HighwayAvx2 ||
                  implementation == Implementation::HighwayAvx512) {
        constexpr auto target{implementation == Implementation::HighwayAvx2 ? HWY_AVX2 : HWY_AVX3};
        if ((hwy::SupportedTargets() & target) == 0) {
            state.SkipWithMessage("Highway target unavailable");
            return;
        }
    }
    auto const count{static_cast<std::size_t>(state.range(0))};
    Buffers buffers{count};
    if constexpr (Coherent) {
        // A coherent fleet keeps the same largest component, so scalar branches
        // are predictable. Keep this alongside the broad orientation distribution.
        for (std::size_t index{}; index < count; ++index) {
            auto const angle{static_cast<float>(index % 100) * 0.001f};
            buffers.rotations[index] = {0, 0, std::sin(angle), std::cos(angle)};
        }
    }
    TransformInput const input{std::as_bytes(std::span{buffers.positions}),
                               std::as_bytes(std::span{buffers.rotations})};
    PackingParameters const parameters{make_vector3f(262144, -262144, 1000000),
                                       make_vector3f(31, -57, 123),
                                       make_vector3f(100, 17, 300)};
    TransformBounds bounds{};
    for (auto _ : state) {
        static_cast<void>(_);
        if constexpr (operation == Operation::Position) {
            constexpr auto kernel{implementation == Implementation::HighwayAvx2
                                      ? experiment::highway_avx2::pack_positions
                                  : implementation == Implementation::HighwayAvx512
                                      ? experiment::highway_avx512::pack_positions
                                  : implementation == Implementation::Avx512
                                      ? experiment::pack_positions_avx512
                                  : implementation == Implementation::Avx2 ? pack_positions_avx2
                                                                           : pack_positions_scalar};
            kernel(input.positions, parameters.position_root, buffers.output);
        } else if constexpr (operation == Operation::Quaternion) {
            constexpr auto kernel{implementation == Implementation::HighwayAvx2
                                      ? experiment::highway_avx2::pack_rotations
                                  : implementation == Implementation::HighwayAvx512
                                      ? experiment::highway_avx512::pack_rotations
                                  : implementation == Implementation::Avx512
                                      ? experiment::pack_rotations_avx512
                                  : implementation == Implementation::Avx2 ? pack_rotations_avx2
                                                                           : pack_rotations_scalar};
            kernel(input.rotations, buffers.output);
        } else {
            constexpr auto kernel{
                implementation == Implementation::HighwayAvx2
                    ? experiment::highway_avx2::pack_transforms
                : implementation == Implementation::HighwayAvx512
                    ? experiment::highway_avx512::pack_transforms
                : implementation == Implementation::Avx512 ? experiment::pack_transforms_avx512
                : implementation == Implementation::Avx2   ? pack_transforms_avx2
                                                           : pack_transforms_scalar};
            kernel(input,
                   parameters,
                   buffers.output,
                   operation == Operation::TransformBounds ? &bounds : nullptr);
        }
        benchmark::DoNotOptimize(buffers.output.data());
        benchmark::DoNotOptimize(bounds);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

template <Operation operation, bool Coherent = false>
auto register_comparison(char const* name) -> void {
    for (auto const implementation : {Implementation::Scalar,
                                      Implementation::Avx2,
                                      Implementation::Avx512,
                                      Implementation::HighwayAvx2,
                                      Implementation::HighwayAvx512}) {
        auto const suffix{implementation == Implementation::HighwayAvx2     ? "/highway-avx2"
                          : implementation == Implementation::HighwayAvx512 ? "/highway-avx512"
                          : implementation == Implementation::Avx512        ? "/avx512"
                          : implementation == Implementation::Avx2          ? "/avx2"
                                                                            : "/scalar"};
        auto const label{std::string{"ismc/"} + name + suffix};
        auto const function{implementation == Implementation::HighwayAvx2
                                ? run<operation, Implementation::HighwayAvx2, Coherent>
                            : implementation == Implementation::HighwayAvx512
                                ? run<operation, Implementation::HighwayAvx512, Coherent>
                            : implementation == Implementation::Avx512
                                ? run<operation, Implementation::Avx512, Coherent>
                            : implementation == Implementation::Avx2
                                ? run<operation, Implementation::Avx2, Coherent>
                                : run<operation, Implementation::Scalar, Coherent>};
        benchmark::RegisterBenchmark(label, function)
            ->Arg(17)
            ->Arg(64)
            ->Arg(256)
            ->Arg(2000)
            ->Arg(4000)
            ->Arg(40000)
            ->UseRealTime();
    }
}
auto register_benchmarks() -> bool {
    register_comparison<Operation::Position>("position");
    register_comparison<Operation::Quaternion>("quaternion");
    register_comparison<Operation::Transform>("transform");
    register_comparison<Operation::TransformBounds>("transform_bounds");
    register_comparison<Operation::Transform, true>("transform_coherent");
    register_comparison<Operation::TransformBounds, true>("transform_bounds_coherent");
    return true;
}
auto const benchmarks_registered{register_benchmarks()};
}
