#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <sandbox/simulation/level_telemetry_block_history_types.h>

#include "sandbox/simulation/memory/GameMemory.h"
#include "sandbox/simulation/telemetry/LevelTelemetryHistory.h"

using FLevelTelemetryHistoryConfig = ml::simulation::LevelTelemetryHistoryConfig;
using FLevelTelemetryBlockHistoryStats = ml::simulation::LevelTelemetryBlockHistoryStats;

class FLevelTelemetryBlockHistory {
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
        for (std::int32_t index{}; index < used_block_count_; ++index) {
            auto const& block{blocks_[index]};
            func(ml::level_telemetry::FHistoryRowsSingleConstView{
                &block.storage, 0, block.storage.num_});
        }
    }

    auto num() const noexcept -> std::int32_t { return row_count_; }
    auto capacity() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(blocks_.size()) * rows_per_block_;
    }
    auto rows_per_block() const noexcept -> std::int32_t { return rows_per_block_; }
    auto retained_block_count() const noexcept -> std::int32_t {
        return static_cast<std::int32_t>(blocks_.size());
    }
    auto block_view(std::int32_t index) const -> ml::level_telemetry::FHistoryRowsSingleConstView;
    auto block_data(std::int32_t index) const -> std::byte const*;
    auto get_stats() const noexcept -> FLevelTelemetryBlockHistoryStats;
  private:
    struct FBlock {
        FBlock(FGameMemoryBlock memory_block, std::int32_t capacity);

        FGameMemoryBlock memory_block{};
        ml::native_soa::StorageState storage{};
    };

    auto acquire_or_reuse_next_block() -> FBlock&;

    FGameMemory& memory_;
    FLevelTelemetryHistoryConfig config_{};
    std::vector<FBlock> blocks_{};
    std::size_t layout_bytes_per_block_{};
    std::size_t logical_payload_bytes_per_row_{};
    std::int32_t rows_per_block_{};
    std::int32_t used_block_count_{};
    std::int32_t peak_block_count_{};
    std::int32_t row_count_{};
};
