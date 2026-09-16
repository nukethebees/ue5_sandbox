#include <benchmark/benchmark.h>

#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/laser_soa.h>

#include <algorithm>
#include <cstdint>

namespace ml::native_soa_benchmarks {
namespace {
template <typename Owner>
auto columns(Owner& owner) {
    if constexpr (requires { owner.get_view().columns(); }) {
        return owner.get_view().columns();
    } else {
        return owner.get_view();
    }
}

template <typename Owner>
void populate(Owner& owner, std::int32_t const count) {
    owner.reserve(count);
    owner.add_defaulted(count);
    auto const view{columns(owner)};
    view.each_column([](auto const column) {
        std::ranges::fill(column, typename decltype(column)::value_type{});
    });
}

template <typename Owner>
void bulk_creation(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    for (auto _ : state) {
        Owner owner;
        owner.reserve(count);
        owner.add_defaulted(count);
        benchmark::DoNotOptimize(owner);
    }
    state.SetItemsProcessed(state.iterations() * count);
}

__declspec(noinline) void fighter_narrow_kernel(ioj::sim::FighterEntityDataView const view) {
    auto const count{view.num()};
    for (std::int32_t index{}; index < count; ++index) {
        view.locations.xs[index] += view.velocities.xs[index] + 1.f;
        view.healths[index] += 1;
    }
}

__declspec(noinline) void laser_narrow_kernel(ioj::sim::lasers::EntitiesView const view) {
    auto const count{view.num()};
    for (std::int32_t index{}; index < count; ++index) {
        view.locations.xs[index] += view.velocities.xs[index] + 1.f;
        view.lifetimes_remaining[index] -= 0.016f;
    }
}

template <typename View>
__declspec(noinline) void broad_kernel(View const view) {
    auto const count{view.num()};
    view.each_column([count](auto const column) {
        for (std::int32_t index{}; index < count; ++index) {
            benchmark::DoNotOptimize(column[index]);
        }
    });
}

template <typename Owner>
void fighter_narrow_iteration(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    Owner owner;
    populate(owner, count);
    auto const view{columns(owner)};
    for (auto _ : state) {
        fighter_narrow_kernel(view);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

template <typename Owner>
void laser_narrow_iteration(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    Owner owner;
    populate(owner, count);
    auto const view{columns(owner)};
    for (auto _ : state) {
        laser_narrow_kernel(view);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

template <typename Owner>
void broad_iteration(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    Owner owner;
    populate(owner, count);
    auto const view{columns(owner)};
    for (auto _ : state) {
        broad_kernel(view);
    }
    state.SetItemsProcessed(state.iterations() * count);
}

constexpr std::int32_t row_count{4096};
using OldFighters = ioj::sim::FighterEntityData;
using NewFighters = ioj::sim::SingleAllocationFighterEntityData;
using OldLasers = ioj::sim::lasers::Entities;
using NewLasers = ioj::sim::lasers::SingleAllocationLaserEntities;

BENCHMARK(bulk_creation<OldFighters>)->Name("Production/Fighter/Old/BulkCreation")->Arg(row_count);
BENCHMARK(bulk_creation<NewFighters>)->Name("Production/Fighter/New/BulkCreation")->Arg(row_count);
BENCHMARK(fighter_narrow_iteration<OldFighters>)
    ->Name("Production/Fighter/Old/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(fighter_narrow_iteration<NewFighters>)
    ->Name("Production/Fighter/New/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(broad_iteration<OldFighters>)
    ->Name("Production/Fighter/Old/BroadIteration")
    ->Arg(row_count);
BENCHMARK(broad_iteration<NewFighters>)
    ->Name("Production/Fighter/New/BroadIteration")
    ->Arg(row_count);

BENCHMARK(bulk_creation<OldLasers>)->Name("Production/Laser/Old/BulkCreation")->Arg(row_count);
BENCHMARK(bulk_creation<NewLasers>)->Name("Production/Laser/New/BulkCreation")->Arg(row_count);
BENCHMARK(laser_narrow_iteration<OldLasers>)
    ->Name("Production/Laser/Old/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(laser_narrow_iteration<NewLasers>)
    ->Name("Production/Laser/New/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(broad_iteration<OldLasers>)->Name("Production/Laser/Old/BroadIteration")->Arg(row_count);
BENCHMARK(broad_iteration<NewLasers>)->Name("Production/Laser/New/BroadIteration")->Arg(row_count);
} // namespace
} // namespace ml::native_soa_benchmarks
