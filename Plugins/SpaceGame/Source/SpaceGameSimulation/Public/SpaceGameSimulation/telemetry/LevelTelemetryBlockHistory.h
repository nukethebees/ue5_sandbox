#pragma once

#include <sandbox/simulation/level_telemetry_block_history_types.h>

#include "SpaceGameSimulation/memory/GameMemory.h"
#include "SpaceGameSimulation/telemetry/LevelTelemetryHistory.h"

using FLevelTelemetryHistoryConfig = ml::simulation::LevelTelemetryHistoryConfig;
using FLevelTelemetryBlockHistoryStats = ml::simulation::LevelTelemetryBlockHistoryStats;

class SPACEGAMESIMULATION_API FLevelTelemetryBlockHistory {
  public:
    FLevelTelemetryBlockHistory(FGameMemory& memory, FLevelTelemetryHistoryConfig config = {});
    FLevelTelemetryBlockHistory(FLevelTelemetryBlockHistory const&) = delete;
    FLevelTelemetryBlockHistory(FLevelTelemetryBlockHistory&&) = delete;
    auto operator=(FLevelTelemetryBlockHistory const&) -> FLevelTelemetryBlockHistory& = delete;
    auto operator=(FLevelTelemetryBlockHistory&&) -> FLevelTelemetryBlockHistory& = delete;

    void reset();
    auto append_uninitialized() -> ml::level_telemetry::FHistoryRowsSingleView;
    auto last_view() -> ml::level_telemetry::FHistoryRowsSingleView;
    auto last_const_view() const -> ml::level_telemetry::FHistoryRowsSingleConstView;

    template <typename Func>
    void for_each_block(Func&& func) const {
        for (int32 index{}; index < used_block_count_; ++index) {
            auto const& block{blocks_[index]};
            func(ml::level_telemetry::FHistoryRowsSingleConstView{
                &block.storage, 0, block.storage.num_});
        }
    }

    auto num() const noexcept -> int32 { return row_count_; }
    auto capacity() const noexcept -> int32 { return blocks_.Num() * rows_per_block_; }
    auto rows_per_block() const noexcept -> int32 { return rows_per_block_; }
    auto retained_block_count() const noexcept -> int32 { return blocks_.Num(); }
    auto block_view(int32 index) const -> ml::level_telemetry::FHistoryRowsSingleConstView;
    auto block_data(int32 index) const -> std::byte const*;
    auto get_stats() const noexcept -> FLevelTelemetryBlockHistoryStats;
  private:
    struct FBlock {
        FBlock(FGameMemoryBlock memory_block, int32 capacity);

        FGameMemoryBlock memory_block{};
        ml::soa_storage::StorageState storage{};
    };

    auto acquire_or_reuse_next_block() -> FBlock&;

    FGameMemory& memory_;
    FLevelTelemetryHistoryConfig config_{};
    TArray<FBlock> blocks_{};
    SIZE_T layout_bytes_per_block_{};
    SIZE_T logical_payload_bytes_per_row_{};
    int32 rows_per_block_{};
    int32 used_block_count_{};
    int32 peak_block_count_{};
    int32 row_count_{};
};
