#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/lasers/laser_entities.h>
#include <ioj/sim/lasers/laser_frame_output.h>
#include <ioj/sim/lasers/laser_spawn_requests.h>
#include <ioj/sim/spinners/spinner_entity_data.h>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <vector>

namespace ioj::native_soa_benchmarks {
namespace {
template <typename Owner>
void populate(Owner& owner, std::int32_t const count) {
    owner.reserve(count);
    owner.add_defaulted(count);
    auto const view{owner.get_view()};
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

struct CompactLaserHitFrameOutput {
    void reset() {
        hits.reset();
        hit_ticks.clear();
        hit_ordinals.clear();
    }
    __declspec(noinline) void append_hits(sim::LaserHitDetailsSingleConstView const new_hits,
                                          sim::SimTick const tick) {
        hits.append_from(new_hits);
        auto const count{new_hits.num()};
        for (std::uint32_t index{}; index < count; ++index) {
            hit_ticks.push_back(tick);
            hit_ordinals.push_back(static_cast<std::int32_t>(index));
        }
    }

    sim::LaserHitDetails hits;
    std::vector<sim::SimTick> hit_ticks;
    std::vector<std::int32_t> hit_ordinals;
};

void compact_laser_hit_accumulation(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    auto const batch{static_cast<std::int32_t>(state.range(1))};
    sim::LaserHitDetails source;
    populate(source, batch);
    CompactLaserHitFrameOutput output;
    for (auto _ : state) {
        output.reset();
        for (std::int32_t first{}; first < count; first += batch) {
            output.append_hits(source.get_const_view(), static_cast<sim::SimTick>(first));
        }
        benchmark::DoNotOptimize(output);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

__declspec(noinline) void fighter_narrow_kernel(sim::FighterEntityData::View const view) {
    auto const locations{view.view_locations().xs()};
    auto const velocities{view.view_velocities().xs()};
    auto const count{view.num()};
    for (std::uint32_t index{}; index < count; ++index) {
        locations[index] += velocities[index] + 1.f;
    }
}

__declspec(noinline) void laser_narrow_kernel(sim::lasers::LaserEntities::View const view) {
    auto const locations{view.view_locations().xs()};
    auto const velocities{view.view_velocities().xs()};
    auto const lifetimes{view.lifetimes_remaining()};
    auto const count{view.num()};
    for (std::uint32_t index{}; index < count; ++index) {
        locations[index] += velocities[index] + 1.f;
        lifetimes[index] -= 0.016f;
    }
}

template <typename View>
__declspec(noinline) void broad_kernel(View const view) {
    auto const count{view.num()};
    view.each_column([count](auto const column) {
        for (std::uint32_t index{}; index < count; ++index) {
            benchmark::DoNotOptimize(column[index]);
        }
    });
}

template <typename Owner>
void fighter_narrow_iteration(benchmark::State& state) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    Owner owner;
    populate(owner, count);
    auto const view{owner.get_view()};
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
    auto const view{owner.get_view()};
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
    auto const view{owner.get_view()};
    for (auto _ : state) {
        broad_kernel(view);
    }
    state.SetItemsProcessed(state.iterations() * count);
}

constexpr std::int32_t row_count{4096};
using NewFighters = sim::FighterEntityData;
using NewLasers = sim::lasers::LaserEntities;
using NewSpinners = sim::SpinnerEntityData;

BENCHMARK(bulk_creation<NewFighters>)->Name("Production/Fighter/New/BulkCreation")->Arg(row_count);
BENCHMARK(fighter_narrow_iteration<NewFighters>)
    ->Name("Production/Fighter/New/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(broad_iteration<NewFighters>)
    ->Name("Production/Fighter/New/BroadIteration")
    ->Arg(row_count);

BENCHMARK(bulk_creation<NewLasers>)->Name("Production/Laser/New/BulkCreation")->Arg(row_count);
BENCHMARK(laser_narrow_iteration<NewLasers>)
    ->Name("Production/Laser/New/NarrowIteration")
    ->Arg(row_count);
BENCHMARK(broad_iteration<NewLasers>)->Name("Production/Laser/New/BroadIteration")->Arg(row_count);

BENCHMARK(bulk_creation<NewSpinners>)->Name("Production/Spinner/New/BulkCreation")->Arg(row_count);

BENCHMARK(compact_laser_hit_accumulation)
    ->Name("Production/LaserHit/New/CompactView/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
} // namespace
} // namespace ioj::native_soa_benchmarks
