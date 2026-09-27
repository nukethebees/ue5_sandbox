#include "sandbox/core/sandbox_ismc_packing.h"

#include <benchmark/benchmark.h>
#include <cpuinfo_x86.h>

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

template <Operation operation, bool Avx2>
auto run(benchmark::State& state) -> void {
    if constexpr (Avx2) {
        if (cpu_features::GetX86Info().features.avx2 == 0) {
            state.SkipWithMessage("AVX2 unavailable");
            return;
        }
    }
    auto const count{static_cast<std::size_t>(state.range(0))};
    Buffers buffers{count};
    TransformInput const input{std::as_bytes(std::span{buffers.positions}),
                               std::as_bytes(std::span{buffers.rotations})};
    PackingParameters const parameters{make_vector3f(262144, -262144, 1000000),
                                       make_vector3f(31, -57, 123),
                                       make_vector3f(100, 17, 300)};
    TransformBounds bounds{};
    for (auto _ : state) {
        static_cast<void>(_);
        if constexpr (operation == Operation::Position) {
            constexpr auto kernel{Avx2 ? pack_positions_avx2 : pack_positions_scalar};
            kernel(input.positions, parameters.position_root, buffers.output);
        } else if constexpr (operation == Operation::Quaternion) {
            constexpr auto kernel{Avx2 ? pack_rotations_avx2 : pack_rotations_scalar};
            kernel(input.rotations, buffers.output);
        } else {
            constexpr auto kernel{Avx2 ? pack_transforms_avx2 : pack_transforms_scalar};
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

template <Operation operation>
auto register_pair(char const* name) -> void {
    for (auto const avx2 : {false, true}) {
        auto const label{std::string{"ismc/"} + name + (avx2 ? "/avx2" : "/scalar")};
        auto const function{avx2 ? run<operation, true> : run<operation, false>};
        benchmark::RegisterBenchmark(label, function)
            ->Arg(64)
            ->Arg(256)
            ->Arg(2000)
            ->Arg(4000)
            ->Arg(40000)
            ->UseRealTime();
    }
}
auto register_benchmarks() -> bool {
    register_pair<Operation::Position>("position");
    register_pair<Operation::Quaternion>("quaternion");
    register_pair<Operation::Transform>("transform");
    register_pair<Operation::TransformBounds>("transform_bounds");
    return true;
}
auto const benchmarks_registered{register_benchmarks()};
}
