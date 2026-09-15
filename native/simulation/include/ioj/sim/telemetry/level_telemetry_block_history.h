#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <ioj/sim/level_telemetry_block_history_types.h>

#include "ioj/sim/memory/game_memory.h"
#include "ioj/sim/telemetry/level_telemetry_history.h"

namespace ioj::sim {

class LevelTelemetryBlockHistory {
  public:
    LevelTelemetryBlockHistory(GameMemory& memory, LevelTelemetryHistoryConfig config = {});
    LevelTelemetryBlockHistory(LevelTelemetryBlockHistory const&) = delete;
    LevelTelemetryBlockHistory(LevelTelemetryBlockHistory&&) = delete;
    auto operator=(LevelTelemetryBlockHistory const&) -> LevelTelemetryBlockHistory& = delete;
    auto operator=(LevelTelemetryBlockHistory&&) -> LevelTelemetryBlockHistory& = delete;

    void reset();
    auto append_uninitialized() -> telemetry::HistoryRowsSingleView;
    auto last_view() -> telemetry::HistoryRowsSingleView;
    auto last_const_view() const -> telemetry::HistoryRowsSingleConstView;

    template <typename Func>
    void for_each_block(Func&& func) const {
        for (std::int32_t index{}; index < used_block_count_; ++index) {
            auto const& block{blocks_[index]};
            func(telemetry::HistoryRowsSingleConstView{&block.storage, 0, block.storage.num_});
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
    auto block_view(std::int32_t index) const -> telemetry::HistoryRowsSingleConstView;
    auto block_data(std::int32_t index) const -> std::byte const*;
    auto get_stats() const noexcept -> LevelTelemetryBlockHistoryStats;
  private:
    struct Block {
        Block(GameMemoryBlock memory_block, std::int32_t capacity);

        GameMemoryBlock memory_block{};
        ml::native_soa::StorageState storage{};
    };

    auto acquire_or_reuse_next_block() -> Block&;

    GameMemory& memory_;
    LevelTelemetryHistoryConfig config_{};
    std::vector<Block> blocks_{};
    std::size_t layout_bytes_per_block_{};
    std::size_t logical_payload_bytes_per_row_{};
    std::int32_t rows_per_block_{};
    std::int32_t used_block_count_{};
    std::int32_t peak_block_count_{};
    std::int32_t row_count_{};
};
} // namespace ioj::sim
