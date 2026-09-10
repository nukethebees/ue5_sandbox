#include <SpaceGameSimulation/simulation/LevelTelemetryManager.h>

#include <SandboxCore/mimalloc_storage_allocator.h>
#include <SandboxCore/time_series_data.h>

#include <HAL/PlatformTime.h>
#include <Misc/AutomationTest.h>
#include <Misc/CommandLine.h>
#include <Misc/Parse.h>

#include "Windows/AllowWindowsPlatformTypes.h"

#include <Psapi.h>
#include <windows.h>

#include "Windows/HideWindowsPlatformTypes.h"

#undef far
#undef near

#include <bit>
#include <type_traits>

namespace level_telemetry_benchmark {
inline constexpr int32 int32_series_count{43};
inline constexpr int32 uint64_series_count{4};
inline constexpr int32 double_series_bit{47};
inline constexpr int32 series_count{48};
inline constexpr SIZE_T row_payload_bytes{228};

struct FWorkload {
    TCHAR const* name{};
    int32 tick_count{};
    uint64 (*mask_for_tick)(int32){};
};

struct FResult {
    double reserve_ms{};
    double append_ms{};
    double max_append_us{};
    double export_ms{};
    double reset_ms{};
    SIZE_T initial_allocated_bytes{};
    SIZE_T peak_allocated_bytes{};
    SIZE_T allocated_bytes{};
    SIZE_T semantic_write_bytes{};
    SIZE_T growth_copy_bytes{};
    uint64 rows{};
    uint64 payload_writes{};
    int32 allocations{};
    int32 growths{};
    uint64 checksum{};
};

auto capacity_for_bytes(SIZE_T const bytes) -> int32 {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    auto low{SIZE_T{0}};
    auto high{static_cast<SIZE_T>(Layout::max_capacity / Layout::capacity_granularity)};
    while (low < high) {
        auto const middle{low + (high - low + 1) / 2};
        if (Layout::layout_bytes(middle) <= bytes) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return static_cast<int32>(low * Layout::capacity_granularity);
}

auto typical_mask(int32 const tick) -> uint64 {
    uint64 mask{};
    if (tick % 5 == 0) {
        mask |= uint64{1} << 0;
        mask |= uint64{1} << (1 + tick / 5 % 5);
        mask |= uint64{1} << (6 + tick / 5 % 30);
    }
    if (tick % 60 == 0) {
        mask |= ((uint64{1} << 10) - 1) << 36;
    }
    if (tick % 3600 == 0) {
        mask |= uint64{1} << double_series_bit;
    }
    return mask;
}

auto sparse_mask(int32 const tick) -> uint64 {
    return tick % 60 == 0 ? uint64{1} << (tick / 60 % series_count) : 0;
}

auto stress_mask(int32 const tick) -> uint64 {
    static_cast<void>(tick);
    return (uint64{1} << series_count) - 1;
}

auto pathological_mask(int32 const tick) -> uint64 {
    return uint64{1} << (tick % series_count);
}

struct FLegacyHistory {
    TStaticArray<ml::XYSeriesData<uint64, int32>, int32_series_count> int32_series{};
    TStaticArray<ml::XYSeriesData<uint64, uint64>, uint64_series_count> uint64_series{};
    ml::XYSeriesData<uint64, double> double_series{};
    int32 allocation_count{};

    void append(uint64 const tick, uint64 mask) {
        while (mask != 0) {
            auto const field{static_cast<int32>(std::countr_zero(mask))};
            if (field < int32_series_count) {
                auto& series{int32_series[field]};
                auto const time_capacity{series.time_capacity()};
                auto const value_capacity{series.value_capacity()};
                series.add(tick, static_cast<int32>(tick + field));
                allocation_count += series.time_capacity() != time_capacity ? 1 : 0;
                allocation_count += series.value_capacity() != value_capacity ? 1 : 0;
            } else if (field < double_series_bit) {
                auto& series{uint64_series[field - int32_series_count]};
                auto const time_capacity{series.time_capacity()};
                auto const value_capacity{series.value_capacity()};
                series.add(tick, tick + field);
                allocation_count += series.time_capacity() != time_capacity ? 1 : 0;
                allocation_count += series.value_capacity() != value_capacity ? 1 : 0;
            } else {
                auto const time_capacity{double_series.time_capacity()};
                auto const value_capacity{double_series.value_capacity()};
                double_series.add(tick, static_cast<double>(tick + field));
                allocation_count += double_series.time_capacity() != time_capacity ? 1 : 0;
                allocation_count += double_series.value_capacity() != value_capacity ? 1 : 0;
            }
            mask &= mask - 1;
        }
    }

    auto allocated_bytes() const -> SIZE_T {
        SIZE_T result{double_series.allocated_bytes()};
        for (auto const& series : int32_series) {
            result += series.allocated_bytes();
        }
        for (auto const& series : uint64_series) {
            result += series.allocated_bytes();
        }
        return result;
    }

    auto checksum() const -> uint64 {
        uint64 result{};
        for (auto const& series : int32_series) {
            for (auto const value : series.values()) {
                result += static_cast<uint64>(value);
            }
        }
        for (auto const& series : uint64_series) {
            for (auto const value : series.values()) {
                result += value;
            }
        }
        for (auto const value : double_series.values()) {
            result += static_cast<uint64>(value);
        }
        return result;
    }

    void reset() {
        for (auto& series : int32_series) {
            series.reset();
        }
        for (auto& series : uint64_series) {
            series.reset();
        }
        double_series.reset();
    }
};

void write_new_payload(ml::level_telemetry::FHistoryRowsView const& rows,
                       int32 const row,
                       int32 const field,
                       uint64 const value) {
    if (field == 0) {
        rows.active_entities[row] = static_cast<int32>(value);
    } else if (field < 6) {
        rows.active_entities_by_type[row][field - 1] = static_cast<int32>(value);
    } else if (field < 36) {
        auto const flat{field - 6};
        rows.active_entities_by_team_and_type[row][flat / 5][flat % 5] = static_cast<int32>(value);
    } else if (field == 36) {
        rows.spawned_entities[row] = static_cast<int32>(value);
    } else if (field == 37) {
        rows.destroyed_entities[row] = static_cast<int32>(value);
    } else if (field == 38) {
        rows.kills[row] = static_cast<int32>(value);
    } else if (field == 39) {
        rows.registry_slot_count[row] = static_cast<int32>(value);
    } else if (field == 40) {
        rows.active_lasers[row] = static_cast<int32>(value);
    } else if (field == 41) {
        rows.lasers_fired[row] = static_cast<int32>(value);
    } else if (field == 42) {
        rows.occupied_spatial_cell_count[row] = static_cast<int32>(value);
    } else if (field == 43) {
        rows.grid_rebuild_count[row] = value;
    } else if (field == 44) {
        rows.range_query_count[row] = value;
    } else if (field == 45) {
        rows.line_trace_count[row] = value;
    } else if (field == 46) {
        rows.sweep_trace_count[row] = value;
    } else {
        rows.requested_time_scale[row] = static_cast<double>(value);
    }
}

auto new_checksum(ml::level_telemetry::FSingleAllocationHistoryRows const& history) -> uint64 {
    auto const rows{history.get_const_view().columns()};
    uint64 result{};
    for (int32 row{}; row < history.num(); ++row) {
        auto mask{rows.validity_masks[row]};
        while (mask != 0) {
            auto const field{static_cast<int32>(std::countr_zero(mask))};
            if (field == 0) {
                result += rows.active_entities[row];
            } else if (field < 6) {
                result += rows.active_entities_by_type[row][field - 1];
            } else if (field < 36) {
                auto const flat{field - 6};
                result += rows.active_entities_by_team_and_type[row][flat / 5][flat % 5];
            } else if (field == 36) {
                result += rows.spawned_entities[row];
            } else if (field == 37) {
                result += rows.destroyed_entities[row];
            } else if (field == 38) {
                result += rows.kills[row];
            } else if (field == 39) {
                result += rows.registry_slot_count[row];
            } else if (field == 40) {
                result += rows.active_lasers[row];
            } else if (field == 41) {
                result += rows.lasers_fired[row];
            } else if (field == 42) {
                result += rows.occupied_spatial_cell_count[row];
            } else if (field == 43) {
                result += rows.grid_rebuild_count[row];
            } else if (field == 44) {
                result += rows.range_query_count[row];
            } else if (field == 45) {
                result += rows.line_trace_count[row];
            } else if (field == 46) {
                result += rows.sweep_trace_count[row];
            } else {
                result += static_cast<uint64>(rows.requested_time_scale[row]);
            }
            mask &= mask - 1;
        }
    }
    return result;
}

auto benchmark_legacy(FWorkload const& workload) -> FResult {
    FLegacyHistory history;
    FResult result;
    auto const started{FPlatformTime::Seconds()};
    for (int32 tick{1}; tick <= workload.tick_count; ++tick) {
        auto const mask{workload.mask_for_tick(tick)};
        if (mask == 0) {
            continue;
        }
        history.append(static_cast<uint64>(tick), mask);
        ++result.rows;
        result.payload_writes += std::popcount(mask);
        auto const int32_writes{std::popcount(mask & ((uint64{1} << int32_series_count) - 1))};
        auto const wide_writes{std::popcount(mask) - int32_writes};
        result.semantic_write_bytes += int32_writes * (sizeof(uint64) + sizeof(int32));
        result.semantic_write_bytes += wide_writes * (sizeof(uint64) + sizeof(uint64));
    }
    result.append_ms = (FPlatformTime::Seconds() - started) * 1000.0;
    result.allocated_bytes = history.allocated_bytes();
    result.peak_allocated_bytes = result.allocated_bytes;
    result.allocations = history.allocation_count;

    auto const export_started{FPlatformTime::Seconds()};
    result.checksum = history.checksum();
    result.export_ms = (FPlatformTime::Seconds() - export_started) * 1000.0;
    auto const reset_started{FPlatformTime::Seconds()};
    history.reset();
    result.reset_ms = (FPlatformTime::Seconds() - reset_started) * 1000.0;
    return result;
}

auto benchmark_new(FWorkload const& workload, SIZE_T const reserve_bytes) -> FResult {
    ml::level_telemetry::FSingleAllocationHistoryRows history;
    FResult result;
    auto const reserve_started{FPlatformTime::Seconds()};
    history.reserve(capacity_for_bytes(reserve_bytes));
    result.reserve_ms = (FPlatformTime::Seconds() - reserve_started) * 1000.0;
    result.allocations = history.capacity() > 0 ? 1 : 0;
    result.initial_allocated_bytes = history.allocated_bytes();
    result.peak_allocated_bytes = result.initial_allocated_bytes;

    auto const started{FPlatformTime::Seconds()};
    for (int32 tick{1}; tick <= workload.tick_count; ++tick) {
        auto const mask{workload.mask_for_tick(tick)};
        if (mask == 0) {
            continue;
        }
        auto const append_started{FPlatformTime::Seconds()};
        auto const previous_capacity{history.capacity()};
        history.add_uninitialised(1);
        if (history.capacity() != previous_capacity) {
            ++result.allocations;
            ++result.growths;
            result.growth_copy_bytes += static_cast<SIZE_T>(history.num() - 1) * row_payload_bytes;
            result.peak_allocated_bytes =
                FMath::Max(result.peak_allocated_bytes, history.allocated_bytes());
        }
        auto const row{history.num() - 1};
        auto rows{history.get_view().columns()};
        rows.completed_ticks[row] = static_cast<uint64>(tick);
        rows.validity_masks[row] = mask;
        auto remaining{mask};
        while (remaining != 0) {
            auto const field{static_cast<int32>(std::countr_zero(remaining))};
            write_new_payload(rows, row, field, static_cast<uint64>(tick + field));
            remaining &= remaining - 1;
        }
        ++result.rows;
        result.payload_writes += std::popcount(mask);
        auto const append_us{(FPlatformTime::Seconds() - append_started) * 1'000'000.0};
        result.max_append_us = FMath::Max(result.max_append_us, append_us);
        auto const int32_writes{std::popcount(mask & ((uint64{1} << int32_series_count) - 1))};
        auto const wide_writes{std::popcount(mask) - int32_writes};
        result.semantic_write_bytes += sizeof(uint64) * 2;
        result.semantic_write_bytes += int32_writes * sizeof(int32);
        result.semantic_write_bytes += wide_writes * sizeof(uint64);
    }
    result.append_ms = (FPlatformTime::Seconds() - started) * 1000.0;
    result.allocated_bytes = history.allocated_bytes();

    auto const export_started{FPlatformTime::Seconds()};
    result.checksum = new_checksum(history);
    result.export_ms = (FPlatformTime::Seconds() - export_started) * 1000.0;
    auto const reset_started{FPlatformTime::Seconds()};
    history.reset();
    result.reset_ms = (FPlatformTime::Seconds() - reset_started) * 1000.0;
    return result;
}

struct FPageStats {
    SIZE_T committed_bytes{};
    SIZE_T resident_bytes{};
    SIZE_T private_resident_bytes{};
};

template <typename T>
auto page_stats(TConstArrayView<T> const values, int32 const capacity) -> FPageStats {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    auto const page_size{static_cast<SIZE_T>(system_info.dwPageSize)};
    auto const begin{reinterpret_cast<UPTRINT>(values.GetData())};
    auto const end{begin + static_cast<SIZE_T>(capacity) * sizeof(T)};
    auto const page_begin{begin & ~(page_size - 1)};
    auto const page_end{Align(end, page_size)};

    FPageStats result;
    for (auto address{page_begin}; address < page_end; address += page_size) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void const*>(address), &memory, sizeof(memory)) != 0 &&
            memory.State == MEM_COMMIT) {
            result.committed_bytes += page_size;
        }
        PSAPI_WORKING_SET_EX_INFORMATION working_set{};
        working_set.VirtualAddress = reinterpret_cast<void*>(address);
        if (K32QueryWorkingSetEx(GetCurrentProcess(), &working_set, sizeof(working_set)) != 0 &&
            working_set.VirtualAttributes.Valid) {
            result.resident_bytes += page_size;
            if (!working_set.VirtualAttributes.Shared) {
                result.private_resident_bytes += page_size;
            }
        }
    }
    return result;
}

auto owner_page_stats(ml::level_telemetry::FSingleAllocationHistoryRows const& history)
    -> FPageStats {
    auto const columns{history.get_const_view().columns()};
    FPageStats total;
    columns.apply_arrays([&](auto const&... arrays) {
        (([&] {
             auto const stats{page_stats(arrays, history.capacity())};
             total.committed_bytes += stats.committed_bytes;
             total.resident_bytes += stats.resident_bytes;
             total.private_resident_bytes += stats.private_resident_bytes;
         }()),
         ...);
    });
    return total;
}

void log_column_page_stats(FAutomationTestBase& test,
                           ml::level_telemetry::FSingleAllocationHistoryRows const& history,
                           TCHAR const* phase) {
    TStaticArray<TCHAR const*, 17> const names{
        TEXT("completed_ticks"),
        TEXT("validity_masks"),
        TEXT("active_entities"),
        TEXT("active_entities_by_type"),
        TEXT("active_entities_by_team_and_type"),
        TEXT("spawned_entities"),
        TEXT("destroyed_entities"),
        TEXT("kills"),
        TEXT("registry_slot_count"),
        TEXT("active_lasers"),
        TEXT("lasers_fired"),
        TEXT("occupied_spatial_cell_count"),
        TEXT("grid_rebuild_count"),
        TEXT("range_query_count"),
        TEXT("line_trace_count"),
        TEXT("sweep_trace_count"),
        TEXT("requested_time_scale"),
    };
    auto const columns{history.get_const_view().columns()};
    int32 column_index{};
    columns.apply_arrays([&](auto const&... arrays) {
        (([&] {
             auto const stats{page_stats(arrays, history.capacity())};
             test.AddInfo(FString::Printf(
                 TEXT("telemetry_column_pages phase=%s column=%s committed_bytes=%llu "
                      "resident_bytes=%llu private_resident_bytes=%llu"),
                 phase,
                 names[column_index],
                 stats.committed_bytes,
                 stats.resident_bytes,
                 stats.private_resident_bytes));
             ++column_index;
         }()),
         ...);
    });
}

void populate_new(ml::level_telemetry::FSingleAllocationHistoryRows& history,
                  FWorkload const& workload) {
    for (int32 tick{1}; tick <= workload.tick_count; ++tick) {
        auto const mask{workload.mask_for_tick(tick)};
        if (mask == 0) {
            continue;
        }
        history.add_uninitialised(1);
        auto const row{history.num() - 1};
        auto rows{history.get_view().columns()};
        rows.completed_ticks[row] = static_cast<uint64>(tick);
        rows.validity_masks[row] = mask;
        auto remaining{mask};
        while (remaining != 0) {
            auto const field{static_cast<int32>(std::countr_zero(remaining))};
            write_new_payload(rows, row, field, static_cast<uint64>(tick + field));
            remaining &= remaining - 1;
        }
    }
}

template <auto const& Column>
void zero_column(std::byte* const data, SIZE_T const blocks, int32 const first, int32 const count) {
    using T = typename std::remove_cvref_t<decltype(Column)>::value_type;
    FMemory::Memzero(data + Column.offset(blocks) + static_cast<SIZE_T>(first) * sizeof(T),
                     static_cast<SIZE_T>(count) * sizeof(T));
}

void zero_row_range(std::byte* const data,
                    SIZE_T const blocks,
                    int32 const first,
                    int32 const count) {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    zero_column<Layout::CompletedTicks>(data, blocks, first, count);
    zero_column<Layout::ValidityMasks>(data, blocks, first, count);
    zero_column<Layout::ActiveEntities>(data, blocks, first, count);
    zero_column<Layout::ActiveEntitiesByType>(data, blocks, first, count);
    zero_column<Layout::ActiveEntitiesByTeamAndType>(data, blocks, first, count);
    zero_column<Layout::SpawnedEntities>(data, blocks, first, count);
    zero_column<Layout::DestroyedEntities>(data, blocks, first, count);
    zero_column<Layout::Kills>(data, blocks, first, count);
    zero_column<Layout::RegistrySlotCount>(data, blocks, first, count);
    zero_column<Layout::ActiveLasers>(data, blocks, first, count);
    zero_column<Layout::LasersFired>(data, blocks, first, count);
    zero_column<Layout::OccupiedSpatialCellCount>(data, blocks, first, count);
    zero_column<Layout::GridRebuildCount>(data, blocks, first, count);
    zero_column<Layout::RangeQueryCount>(data, blocks, first, count);
    zero_column<Layout::LineTraceCount>(data, blocks, first, count);
    zero_column<Layout::SweepTraceCount>(data, blocks, first, count);
    zero_column<Layout::RequestedTimeScale>(data, blocks, first, count);
}

void run_chunk_zero_probe(FAutomationTestBase& token,
                          SIZE_T const chunk_bytes,
                          std::byte* const data,
                          int32 const capacity,
                          SIZE_T const blocks,
                          SIZE_T const allocation_bytes,
                          FPageStats const pages_before) {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    auto chunk_rows{static_cast<int32>(chunk_bytes / row_payload_bytes)};
    chunk_rows =
        FMath::Max(chunk_rows / Layout::capacity_granularity * Layout::capacity_granularity,
                   Layout::capacity_granularity);
    double first_zero_ms{};
    double total_zero_ms{};
    double max_later_zero_ms{};
    int32 chunk_count{};
    for (int32 first{}; first < capacity; first += chunk_rows) {
        auto const count{FMath::Min(chunk_rows, capacity - first)};
        auto const started{FPlatformTime::Seconds()};
        zero_row_range(data, blocks, first, count);
        auto const elapsed_ms{(FPlatformTime::Seconds() - started) * 1000.0};
        if (first == 0) {
            first_zero_ms = elapsed_ms;
        } else {
            max_later_zero_ms = FMath::Max(max_later_zero_ms, elapsed_ms);
        }
        total_zero_ms += elapsed_ms;
        ++chunk_count;
    }
    auto const pages_after{
        page_stats(TConstArrayView<std::byte>{data, static_cast<int32>(allocation_bytes)},
                   static_cast<int32>(allocation_bytes))};

    token.AddInfo(FString::Printf(
        TEXT("telemetry_chunk_zero chunk_bytes=%llu chunk_rows=%d chunks=%d allocation_bytes=%llu "
             "first_zero_ms=%.6f max_later_zero_ms=%.6f total_zero_ms=%.6f "
             "committed_before=%llu resident_before=%llu private_before=%llu "
             "committed_after=%llu resident_after=%llu private_after=%llu"),
        chunk_bytes,
        chunk_rows,
        chunk_count,
        allocation_bytes,
        first_zero_ms,
        max_later_zero_ms,
        total_zero_ms,
        pages_before.committed_bytes,
        pages_before.resident_bytes,
        pages_before.private_resident_bytes,
        pages_after.committed_bytes,
        pages_after.resident_bytes,
        pages_after.private_resident_bytes));
}

void run_chunk_zero_probes(FAutomationTestBase& token) {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    constexpr SIZE_T reserve_bytes{50u * 1024u * 1024u};
    auto const capacity{capacity_for_bytes(reserve_bytes)};
    auto const blocks{static_cast<SIZE_T>(capacity / Layout::capacity_granularity)};
    auto const allocation_bytes{Layout::layout_bytes(blocks)};
    TStaticArray<SIZE_T, 3> const chunk_bytes{64u << 10, 256u << 10, 1u << 20};
    TStaticArray<std::byte*, 3> allocations{};
    TStaticArray<FPageStats, 3> pages_before{};

    for (int32 index{}; index < allocations.Num(); ++index) {
        allocations[index] = ml::soa_storage::MimallocStorageAllocator::allocate(
            allocation_bytes, static_cast<uint32>(Layout::allocation_alignment));
        pages_before[index] = page_stats(
            TConstArrayView<std::byte>{allocations[index], static_cast<int32>(allocation_bytes)},
            static_cast<int32>(allocation_bytes));
    }

    for (int32 index{}; index < allocations.Num(); ++index) {
        run_chunk_zero_probe(token,
                             chunk_bytes[index],
                             allocations[index],
                             capacity,
                             blocks,
                             allocation_bytes,
                             pages_before[index]);
    }

    for (auto* const allocation : allocations) {
        ml::soa_storage::MimallocStorageAllocator::free(allocation);
    }
}

void log_result(FAutomationTestBase& test,
                TCHAR const* implementation,
                FWorkload const& workload,
                SIZE_T const reserve_bytes,
                FResult const& result) {
    test.AddInfo(FString::Printf(
        TEXT("telemetry_storage implementation=%s workload=%s reserve_bytes=%llu rows=%llu "
             "payload_writes=%llu reserve_ms=%.6f append_ms=%.6f max_append_us=%.6f "
             "export_ms=%.6f reset_ms=%.6f "
             "allocations=%d growths=%d initial_allocated_bytes=%llu peak_allocated_bytes=%llu "
             "allocated_bytes=%llu semantic_write_bytes=%llu "
             "growth_copy_bytes=%llu checksum=%llu manager_size=%llu"),
        implementation,
        workload.name,
        reserve_bytes,
        result.rows,
        result.payload_writes,
        result.reserve_ms,
        result.append_ms,
        result.max_append_us,
        result.export_ms,
        result.reset_ms,
        result.allocations,
        result.growths,
        result.initial_allocated_bytes,
        result.peak_allocated_bytes,
        result.allocated_bytes,
        result.semantic_write_bytes,
        result.growth_copy_bytes,
        result.checksum,
        sizeof(FLevelTelemetryManager)));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelTelemetryStorageBenchmark,
                                 "SandboxBenchmarks.LevelTelemetryStorage",
                                 EAutomationTestFlags::EditorContext |
                                     EAutomationTestFlags::EngineFilter)

auto FLevelTelemetryStorageBenchmark::RunTest(FString const&) -> bool {
    using namespace level_telemetry_benchmark;
    int32 samples{7};
    FParse::Value(FCommandLine::Get(), TEXT("SandboxTelemetryBenchmarkSamples="), samples);
    samples = FMath::Max(samples, 1);

    TStaticArray<FWorkload, 4> const workloads{
        FWorkload{TEXT("typical"), 18'000, typical_mask},
        FWorkload{TEXT("sparse"), 18'000, sparse_mask},
        FWorkload{TEXT("stress"), 18'000, stress_mask},
        FWorkload{TEXT("pathological_60min_60hz"), 216'000, pathological_mask},
    };
    TStaticArray<SIZE_T, 2> const reserves{1024u * 1024u, 50u * 1024u * 1024u};

    // Keep the first large allocation alive so later benchmark allocations cannot cause its
    // untouched pages to become resident through allocator reuse.
    ml::level_telemetry::FSingleAllocationHistoryRows residency_probe;
    auto const residency_reserve_started{FPlatformTime::Seconds()};
    residency_probe.reserve(capacity_for_bytes(SIZE_T{50} << 20));
    auto const residency_reserve_ms{(FPlatformTime::Seconds() - residency_reserve_started) *
                                    1000.0};
    AddInfo(FString::Printf(TEXT("telemetry_natural_touch phase=after_reserve reserve_ms=%.6f "
                                 "allocated_bytes=%llu capacity=%d"),
                            residency_reserve_ms,
                            residency_probe.allocated_bytes(),
                            residency_probe.capacity()));
    log_column_page_stats(*this, residency_probe, TEXT("after_reserve"));
    populate_new(residency_probe, workloads[0]);
    log_column_page_stats(*this, residency_probe, TEXT("after_typical"));
    run_chunk_zero_probes(*this);

    for (auto const& workload : workloads) {
        for (int32 sample{}; sample < samples; ++sample) {
            auto const legacy{benchmark_legacy(workload)};
            log_result(*this, TEXT("legacy_xy"), workload, 0, legacy);
            for (auto const reserve_bytes : reserves) {
                auto const single{benchmark_new(workload, reserve_bytes)};
                TestEqual(TEXT("Legacy and single-allocation payload checksums agree"),
                          single.checksum,
                          legacy.checksum);
                log_result(*this,
                           reserve_bytes == reserves[0] ? TEXT("single_geometric")
                                                        : TEXT("single_large_natural_touch"),
                           workload,
                           reserve_bytes,
                           single);
            }
        }
    }

    for (SIZE_T const reserve_bytes : {SIZE_T{8} << 20,
                                       SIZE_T{16} << 20,
                                       SIZE_T{32} << 20,
                                       SIZE_T{50} << 20,
                                       SIZE_T{64} << 20}) {
        ml::level_telemetry::FSingleAllocationHistoryRows history;
        auto const started{FPlatformTime::Seconds()};
        history.reserve(capacity_for_bytes(reserve_bytes));
        auto const reserve_ms{(FPlatformTime::Seconds() - started) * 1000.0};
        auto const pages{owner_page_stats(history)};
        AddInfo(FString::Printf(
            TEXT("telemetry_reserve requested_bytes=%llu allocated_bytes=%llu capacity=%d "
                 "reserve_ms=%.6f committed_column_page_bytes=%llu resident_column_page_bytes=%llu "
                 "private_resident_column_page_bytes=%llu"),
            reserve_bytes,
            history.allocated_bytes(),
            history.capacity(),
            reserve_ms,
            pages.committed_bytes,
            pages.resident_bytes,
            pages.private_resident_bytes));
    }

    return true;
}
