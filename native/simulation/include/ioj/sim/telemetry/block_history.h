#pragma once

#include "ioj/sim/memory/game_memory.h"
#include "ioj/sim/telemetry/history.h"
#include <ioj/sim/telemetry/block_history_types.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ioj::sim {

class LevelTelemetryBlockHistory {
  public:
    LevelTelemetryBlockHistory(GameMemory& memory, LevelTelemetryHistoryConfig config = {});
    LevelTelemetryBlockHistory(LevelTelemetryBlockHistory const&) = delete;
    LevelTelemetryBlockHistory(LevelTelemetryBlockHistory&&) = delete;
    auto operator=(LevelTelemetryBlockHistory const&) -> LevelTelemetryBlockHistory& = delete;
    auto operator=(LevelTelemetryBlockHistory&&) -> LevelTelemetryBlockHistory& = delete;

    void reset();
    auto append_uninitialized() -> telemetry::history_rows::View;
    auto last_view() -> telemetry::history_rows::View;
    auto last_const_view() const -> telemetry::history_rows::ConstView;

    template <typename Func>
    void for_each_block(Func&& func) const {
        for (std::uint32_t index{}; index < used_block_count_; ++index) {
            auto const& block{blocks_[index]};
            func(telemetry::history_rows::ConstView{&block.storage, 0, block.storage.num_});
        }
    }

    auto num() const noexcept -> std::uint32_t { return row_count_; }
    auto capacity() const noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(blocks_.size()) * rows_per_block_;
    }
    auto rows_per_block() const noexcept -> std::uint32_t { return rows_per_block_; }
    auto retained_block_count() const noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(blocks_.size());
    }
    auto block_view(std::uint32_t index) const -> telemetry::history_rows::ConstView;
    auto block_data(std::uint32_t index) const -> std::byte const*;
    auto get_stats() const noexcept -> LevelTelemetryBlockHistoryStats;
  private:
    struct Block {
        Block(memory::Block memory_block, std::uint32_t capacity);

        memory::Block memory_block{};
        native_soa::StorageState storage{};
    };

    auto acquire_or_reuse_next_block() -> Block&;

    GameMemory& memory_;
    LevelTelemetryHistoryConfig config_{};
    std::vector<Block> blocks_{};
    std::size_t layout_bytes_per_block_{};
    std::size_t logical_payload_bytes_per_row_{};
    std::uint32_t rows_per_block_{};
    std::uint32_t used_block_count_{};
    std::uint32_t peak_block_count_{};
    std::uint32_t row_count_{};
};
} // namespace ioj::sim
