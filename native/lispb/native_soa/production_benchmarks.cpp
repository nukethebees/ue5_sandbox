#include <benchmark/benchmark.h>

#include <ioj/sim/fighter_entity_data.h>
#include <ioj/sim/laser_frame_output.h>
#include <ioj/sim/laser_soa.h>
#include <ioj/sim/spinner_entity_data.h>

#include <algorithm>
#include <cstdint>
#include <vector>

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

struct OldLaserHitFrameOutput {
    void reset() {
        hits.reset();
        hit_ticks.clear();
        hit_ordinals.clear();
    }
    __declspec(noinline) void append_hits(ioj::sim::LaserHitDetailsConstView const new_hits,
                                          ioj::sim::SimTick const tick) {
        hits.append_from(new_hits);
        auto const count{new_hits.num()};
        for (std::int32_t index{}; index < count; ++index) {
            hit_ticks.push_back(tick);
            hit_ordinals.push_back(index);
        }
    }

    ioj::sim::LaserHitDetails hits;
    std::vector<ioj::sim::SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};

struct ManualLaserHitFrameOutput {
    void reset() {
        hits.reset();
        hit_ticks.clear();
        hit_ordinals.clear();
    }
    __declspec(noinline) void append_hits(ioj::sim::LaserHitDetailsConstView const new_hits,
                                          ioj::sim::SimTick const tick) {
        auto const count{new_hits.num()};
        auto const first{hits.num()};
        hits.add_uninitialised(count);
        auto const appended{hits.get_view(first, count).columns()};
        std::ranges::copy(new_hits.locations.xs_span(), appended.locations.xs_span().begin());
        std::ranges::copy(new_hits.locations.ys_span(), appended.locations.ys_span().begin());
        std::ranges::copy(new_hits.locations.zs_span(), appended.locations.zs_span().begin());
        std::ranges::copy(new_hits.emission_directions.xs_span(),
                          appended.emission_directions.xs_span().begin());
        std::ranges::copy(new_hits.emission_directions.ys_span(),
                          appended.emission_directions.ys_span().begin());
        std::ranges::copy(new_hits.emission_directions.zs_span(),
                          appended.emission_directions.zs_span().begin());
        std::ranges::copy(new_hits.sources, appended.sources.begin());

        for (std::int32_t index{}; index < count; ++index) {
            hit_ticks.push_back(tick);
            hit_ordinals.push_back(index);
        }
    }

    ioj::sim::SingleAllocationLaserHitDetails hits;
    std::vector<ioj::sim::SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};

struct CompactLaserHitFrameOutput {
    void reset() {
        hits.reset();
        hit_ticks.clear();
        hit_ordinals.clear();
    }
    __declspec(noinline) void append_hits(ioj::sim::LaserHitDetailsSingleConstView const new_hits,
                                          ioj::sim::SimTick const tick) {
        hits.append_from(new_hits);
        auto const count{new_hits.num()};
        for (std::int32_t index{}; index < count; ++index) {
            hit_ticks.push_back(tick);
            hit_ordinals.push_back(index);
        }
    }

    ioj::sim::SingleAllocationLaserHitDetails hits;
    std::vector<ioj::sim::SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};

template <typename Output>
void laser_hit_accumulation(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    auto const batch{static_cast<std::int32_t>(state.range(1))};
    ioj::sim::LaserHitDetails source;
    populate(source, batch);
    Output output;
    for (auto _ : state) {
        output.reset();
        for (std::int32_t first{}; first < count; first += batch) {
            output.append_hits(source.get_const_view(), static_cast<ioj::sim::SimTick>(first));
        }
        benchmark::DoNotOptimize(output);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

void compact_laser_hit_accumulation(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    auto const batch{static_cast<std::int32_t>(state.range(1))};
    ioj::sim::SingleAllocationLaserHitDetails source;
    populate(source, batch);
    CompactLaserHitFrameOutput output;
    for (auto _ : state) {
        output.reset();
        for (std::int32_t first{}; first < count; first += batch) {
            output.append_hits(source.get_const_view(), static_cast<ioj::sim::SimTick>(first));
        }
        benchmark::DoNotOptimize(output);
        benchmark::ClobberMemory();
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
using OldSpinners = ioj::sim::SpinnerEntityData;
using NewSpinners = ioj::sim::SingleAllocationSpinnerEntityData;

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

BENCHMARK(bulk_creation<OldSpinners>)->Name("Production/Spinner/Old/BulkCreation")->Arg(row_count);
BENCHMARK(bulk_creation<NewSpinners>)->Name("Production/Spinner/New/BulkCreation")->Arg(row_count);

BENCHMARK(laser_hit_accumulation<OldLaserHitFrameOutput>)
    ->Name("Production/LaserHit/Old/OrdinaryView/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
BENCHMARK(laser_hit_accumulation<ioj::sim::lasers::FrameOutput>)
    ->Name("Production/LaserHit/New/OrdinaryView/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
BENCHMARK(laser_hit_accumulation<ManualLaserHitFrameOutput>)
    ->Name("Production/LaserHit/New/ManualOrdinaryView/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
BENCHMARK(compact_laser_hit_accumulation)
    ->Name("Production/LaserHit/New/CompactView/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
} // namespace
} // namespace ml::native_soa_benchmarks
