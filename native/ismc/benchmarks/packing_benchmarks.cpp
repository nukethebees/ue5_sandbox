#include "ioj/ismc/packing.h"

#include <benchmark/benchmark.h>

#include <array>
#include <random>
#include <string>
#include <vector>

namespace ioj::ismc::packing_benchmarks {
enum class Operation {
    Position,
    Quaternion,
    Transform,
    TransformBounds,
};

enum class Implementation {
    Scalar,
    Production,
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
            auto const q{ioj::normalized(
                ioj::Quaternion4f{normal(random), normal(random), normal(random), normal(random)})};
            rotations[index] = {q.x(), q.y(), q.z(), q.w()};
        }
    }
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 4>> rotations;
    std::vector<PackedTransform> output;
};

template <Operation operation, Implementation implementation, bool Coherent = false>
auto run(benchmark::State& state) -> void {
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
    PackingParameters const parameters{ioj::Vector3f{262144, -262144, 1000000},
                                       ioj::Vector3f{31, -57, 123},
                                       ioj::Vector3f{100, 17, 300}};
    TransformPacker const packer{
        parameters,
        operation == Operation::TransformBounds ? BoundsMode::Calculate : BoundsMode::Skip,
        implementation == Implementation::Production ? PackingBackend::Production
                                                     : PackingBackend::Scalar};
    TransformBounds bounds{};
    for (auto _ : state) {
        static_cast<void>(_);
        if constexpr (operation == Operation::Position) {
            packer.pack_positions(input.positions, buffers.output);
        } else if constexpr (operation == Operation::Quaternion) {
            packer.pack_rotations(input.rotations, buffers.output);
        } else {
            bounds = packer.pack_transforms(input, buffers.output);
        }
        benchmark::DoNotOptimize(buffers.output.data());
        benchmark::DoNotOptimize(bounds);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

template <Operation operation, bool Coherent = false>
auto register_comparison(char const* name) -> void {
    for (auto const implementation : {Implementation::Scalar, Implementation::Production}) {
        auto const suffix{implementation == Implementation::Production ? "/production" : "/scalar"};
        auto const label{std::string{"ismc/"} + name + suffix};
        auto const function{implementation == Implementation::Production
                                ? run<operation, Implementation::Production, Coherent>
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
