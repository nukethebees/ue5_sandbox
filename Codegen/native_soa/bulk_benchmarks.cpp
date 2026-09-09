#include <benchmark/benchmark.h>
#include <native_soa_types.h>
#include <string>

namespace ml::native_soa_bulk_benchmarks {
using namespace native_experiment;
inline constexpr std::int32_t row_count{65536};

template <typename Owner>
void append(benchmark::State& state, bool reserved, std::int32_t batch) {
    Owner source;
    source.set_num(row_count);
    auto const view{std::as_const(source).get_view()};
    Owner reused;
    if (reserved) {
        reused.reserve(row_count);
    }
    for (auto _ : state) {
        Owner fresh;
        auto& destination{reserved ? reused : fresh};
        destination.reset();
        for (std::int32_t first{}; first < row_count; first += batch) {
            destination.append_from(view.slice(first, batch));
        }
        benchmark::DoNotOptimize(destination);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * row_count);
}

template <typename Owner>
void remove(benchmark::State& state, bool scattered) {
    Owner owner;
    owner.set_num(row_count);
    std::vector<std::int32_t> indices;
    for (std::int32_t row{row_count / 4 - 1}; row >= 0; --row) {
        indices.push_back(scattered ? row * 4 : row);
    }
    for (auto _ : state) {
        owner.set_num(row_count);
        if constexpr (requires { owner.remove_at_swap(std::span<std::int32_t const>{indices}); }) {
            owner.remove_at_swap(std::span<std::int32_t const>{indices});
        } else {
            auto const columns{owner.get_view()};
            soa_storage_detail::for_each_removal_run(
                owner.num(),
                indices,
                native_soa::require,
                [&](std::int32_t destination, std::int32_t source, std::int32_t count) {
                    columns.each_column([&](auto column) {
                        std::memcpy(column.data() + destination,
                                    column.data() + source,
                                    static_cast<std::size_t>(count) * sizeof(column[0]));
                    });
                });
            owner.set_num(owner.num() - static_cast<std::int32_t>(indices.size()));
        }
        benchmark::DoNotOptimize(owner);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(indices.size()));
}

template <typename Owner>
void views(benchmark::State& state, bool materialize) {
    Owner owner;
    owner.set_num(row_count);
    for (auto _ : state) {
        for (int iteration{}; iteration < 4096; ++iteration) {
            auto view{std::as_const(owner).get_view()};
            if constexpr (requires { view.columns(); }) {
                if (materialize) {
                    benchmark::DoNotOptimize(view.columns());
                } else {
                    benchmark::DoNotOptimize(view);
                }
            } else {
                benchmark::DoNotOptimize(view);
            }
        }
    }
    state.SetItemsProcessed(state.iterations() * 4096);
}

template <typename Owner>
void register_owner(std::string const& label) {
    for (bool reserved : {false, true}) {
        for (std::int32_t batch : {64, row_count}) {
            auto const name{label + "/append_from_" + (reserved ? "reserved_" : "natural_") +
                            std::to_string(batch)};
            benchmark::RegisterBenchmark(
                name.c_str(),
                [=](benchmark::State& state) { append<Owner>(state, reserved, batch); })
                ->Arg(row_count)
                ->UseRealTime();
        }
    }
    for (bool scattered : {false, true}) {
        auto const name{label + "/remove_indices_" + (scattered ? "scattered" : "clustered")};
        benchmark::RegisterBenchmark(
            name.c_str(), [=](benchmark::State& state) { remove<Owner>(state, scattered); })
            ->Arg(row_count)
            ->UseRealTime();
    }
    for (bool materialize : {false, true}) {
        auto const name{label + (materialize ? "/materialize_columns" : "/view_handles")};
        benchmark::RegisterBenchmark(
            name.c_str(), [=](benchmark::State& state) { views<Owner>(state, materialize); })
            ->Arg(row_count)
            ->UseRealTime();
    }
}

inline auto const registered{[] {
    register_owner<EntityData>("Vector");
    register_owner<SingleAllocationEntityData>("Single");
    return true;
}()};
}
