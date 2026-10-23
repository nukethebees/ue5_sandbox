#include <ioj/sim/fighters/fighter_entity_data.h>
#include <ioj/sim/lasers/laser_entities.h>
#include <ioj/sim/lasers/laser_frame_output.h>
#include <ioj/sim/spinners/spinner_entity_data.h>

#include <benchmark/benchmark.h>

#include <cstdint>

namespace ioj::native_soa_benchmarks {
inline constexpr std::uint32_t row_count{4096};

namespace {
template <typename Owner>
void bulk_creation(benchmark::State& state) {
    auto const count{static_cast<std::uint32_t>(state.range(0))};
    for (auto _ : state) {
        Owner owner;
        owner.reserve(count);
        owner.add_defaulted(count);
        benchmark::DoNotOptimize(owner);
    }
    state.SetItemsProcessed(state.iterations() * count);
}

void laser_hit_accumulation(benchmark::State& state) {
    auto const count{static_cast<std::uint32_t>(state.range(0))};
    auto const batch{static_cast<std::uint32_t>(state.range(1))};
    sim::LaserHitDetails source;
    source.set_num(batch);
    auto const hits{source.get_const_view()};
    sim::lasers::FrameOutput output;
    for (auto _ : state) {
        output.reset();
        for (std::uint32_t first{}; first < count; first += batch) {
            output.append_hits(hits, static_cast<sim::SimTick>(first));
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

void fighter_narrow_iteration(benchmark::State& state) {
    auto const count{static_cast<std::uint32_t>(state.range(0))};
    sim::FighterEntityData owner;
    owner.set_num(count);
    auto const view{owner.get_view()};
    for (auto _ : state) {
        fighter_narrow_kernel(view);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

void laser_narrow_iteration(benchmark::State& state) {
    auto const count{static_cast<std::uint32_t>(state.range(0))};
    sim::lasers::LaserEntities owner;
    owner.set_num(count);
    auto const view{owner.get_view()};
    for (auto _ : state) {
        laser_narrow_kernel(view);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * count);
}

template <typename Owner>
void broad_iteration(benchmark::State& state) {
    auto const count{static_cast<std::uint32_t>(state.range(0))};
    Owner owner;
    owner.set_num(count);
    auto const view{owner.get_view()};
    for (auto _ : state) {
        broad_kernel(view);
    }
    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(bulk_creation<sim::FighterEntityData>)
    ->Name("Production/Fighter/BulkCreation")
    ->Arg(row_count);
BENCHMARK(fighter_narrow_iteration)->Name("Production/Fighter/NarrowIteration")->Arg(row_count);
BENCHMARK(broad_iteration<sim::FighterEntityData>)
    ->Name("Production/Fighter/BroadIteration")
    ->Arg(row_count);

BENCHMARK(bulk_creation<sim::lasers::LaserEntities>)
    ->Name("Production/Laser/BulkCreation")
    ->Arg(row_count);
BENCHMARK(laser_narrow_iteration)->Name("Production/Laser/NarrowIteration")->Arg(row_count);
BENCHMARK(broad_iteration<sim::lasers::LaserEntities>)
    ->Name("Production/Laser/BroadIteration")
    ->Arg(row_count);

BENCHMARK(bulk_creation<sim::SpinnerEntityData>)
    ->Name("Production/Spinner/BulkCreation")
    ->Arg(row_count);

BENCHMARK(laser_hit_accumulation)
    ->Name("Production/LaserHit/AppendReset")
    ->Args({row_count, 1})
    ->Args({row_count, 16})
    ->Args({row_count, 64})
    ->Args({row_count, 256})
    ->Args({row_count, 4096});
} // namespace
} // namespace ioj::native_soa_benchmarks
