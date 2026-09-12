#include <benchmark/benchmark.h>
#include <native_soa_types.h>

#include <string>

namespace ml::native_soa_benchmarks {
using namespace native_experiment;
template <typename View>
auto array_columns(View view) {
    if constexpr (requires { view.columns(); }) {
        return view.columns();
    } else {
        return view;
    }
}

enum class Operation {
    Reserve,
    Growth,
    Append,
    ReservedAppend,
    SetNum,
    Reset,
    Remove,
    Iterate,
    Views,
    Uninitialised
};

void initialise(EntityDataView view) {
    auto const count{view.num()};
    for (std::int32_t i{}; i < count; ++i) {
        view.healths[i] = i;
        view.velocities.xs[i] = 1.f;
        view.velocities.ys[i] = 2.f;
        view.velocities.zs[i] = 3.f;
    }
}

// Both representations call the same kernel with the same span-based view.
void iterate(EntityDataView view) {
    auto const count{view.num()};
    for (std::int32_t i{}; i < count; ++i) {
        view.locations.xs[i] += view.velocities.xs[i] * .125f;
        view.locations.ys[i] += view.velocities.ys[i] * .125f;
        view.locations.zs[i] += view.velocities.zs[i] * .125f;
    }
    benchmark::ClobberMemory();
}

template <typename Owner>
void run(benchmark::State& state, Operation operation, std::int32_t batch) {
    auto const count{static_cast<std::int32_t>(state.range(0))};
    Owner owner;
    auto const lifecycle{operation == Operation::Reserve || operation == Operation::Growth ||
                         operation == Operation::Append};
    auto prepare = [&] {
        if (operation == Operation::Reserve || operation == Operation::Append) {
            return;
        }
        owner.reserve(count);
        if (operation == Operation::Growth || operation == Operation::Remove ||
            operation == Operation::Iterate || operation == Operation::Views) {
            owner.set_num(count);
            initialise(array_columns(owner.get_view()));
        }
    };
    prepare();
    auto view{array_columns(owner.get_view())};
    for (auto _ : state) {
        // Pause/resume once per complete container operation, never once per appended row.
        if (operation != Operation::Iterate && operation != Operation::Views) {
            state.PauseTiming();
            if (lifecycle) {
                owner = Owner{};
                prepare();
            } else if (operation == Operation::Remove) {
                owner.set_num(count);
            } else {
                owner.reset();
            }
            state.ResumeTiming();
        }
        switch (operation) {
            case Operation::Reserve:
                owner.reserve(count);
                break;
            case Operation::Growth:
                owner.reserve(count * 2 + 1);
                break;
            case Operation::Append:
            case Operation::ReservedAppend:
                for (std::int32_t i{}; i < count; i += batch) {
                    owner.add_defaulted(std::min(batch, count - i));
                }
                break;
            case Operation::Uninitialised:
                if constexpr (requires { owner.add_uninitialised(count); }) {
                    for (std::int32_t i{}; i < count; i += batch) {
                        owner.add_uninitialised(std::min(batch, count - i));
                    }
                }
                break;
            case Operation::SetNum:
                owner.set_num(count);
                break;
            case Operation::Reset:
                for (int i{}; i < 16; ++i) {
                    owner.reset();
                    owner.set_num(count);
                    benchmark::ClobberMemory();
                }
                break;
            case Operation::Remove:
                for (std::int32_t i{}; i < count / 4; ++i) {
                    owner.remove_at_swap(i, 1);
                }
                break;
            case Operation::Iterate:
                iterate(view);
                break;
            case Operation::Views:
                for (int i{}; i < 4096; ++i) {
                    benchmark::DoNotOptimize(array_columns(owner.get_view()));
                }
                break;
        }
        benchmark::DoNotOptimize(owner);
        benchmark::ClobberMemory();
    }
    auto const items{operation == Operation::Views    ? std::int64_t{4096}
                     : operation == Operation::Reset  ? std::int64_t{count} * 16
                     : operation == Operation::Remove ? std::int64_t{count / 4}
                                                      : std::int64_t{count}};
    state.SetItemsProcessed(state.iterations() * items);
    std::size_t bytes{};
    std::size_t blocks{};
    if constexpr (requires { owner.allocated_bytes(); }) {
        bytes = owner.allocated_bytes();
        blocks = owner.capacity() > 0 ? 1 : 0;
    } else {
        owner.each_column([&](auto const& column) {
            bytes += column.capacity() *
                     sizeof(typename std::remove_cvref_t<decltype(column)>::value_type);
            blocks += column.capacity() > 0 ? 1 : 0;
        });
    }
    state.counters["retained_bytes"] = static_cast<double>(bytes);
    state.counters["retained_blocks"] = static_cast<double>(blocks);
    std::size_t row_bytes{};
    array_columns(owner.get_view()).each_column([&](auto column) {
        row_bytes += sizeof(typename decltype(column)::value_type);
    });
    state.counters["live_bytes"] =
        static_cast<double>(row_bytes * static_cast<std::size_t>(owner.num()));
    state.counters["unused_bytes"] =
        static_cast<double>(bytes - row_bytes * static_cast<std::size_t>(owner.num()));
    state.counters["owner_bytes"] = sizeof(Owner);
}

template <typename Owner>
void register_owner(char const* name) {
    auto add = [&](char const* operation_name, Operation operation, std::int32_t batch = 1) {
        auto const benchmark_name{std::string{name} + "/" + operation_name};
        benchmark::RegisterBenchmark(
            benchmark_name.c_str(),
            [=](benchmark::State& state) { run<Owner>(state, operation, batch); })
            ->Arg(4096)
            ->Arg(65536)
            ->Arg(1048576)
            ->UseRealTime();
    };
    add("reserve", Operation::Reserve);
    add("populated_growth", Operation::Growth);
    add("defaulted_append_1", Operation::Append);
    add("defaulted_append_64", Operation::Append, 64);
    add("reserved_defaulted_append_1", Operation::ReservedAppend);
    add("reserved_defaulted_append_64", Operation::ReservedAppend, 64);
    add("set_num", Operation::SetNum);
    add("reset_refill_16", Operation::Reset);
    add("remove_swap", Operation::Remove);
    add("iterate", Operation::Iterate);
    add("views_4096", Operation::Views);
    if constexpr (requires(Owner& owner) { owner.add_uninitialised(1); }) {
        add("reserved_uninitialised_append_1", Operation::Uninitialised);
    }
}

inline auto const registered{[] {
    benchmark::AddCustomContext("soa_schema", "fighter equivalent: 53 leaves, 195 bytes per row");
    benchmark::AddCustomContext("soa_allocation",
                                "std::vector default allocator vs aligned operator new/delete");
    benchmark::AddCustomContext("soa_capacity_granularity", "64");
    register_owner<EntityData>("Vector");
    register_owner<SingleAllocationEntityData>("Single");
    return true;
}()};
}
