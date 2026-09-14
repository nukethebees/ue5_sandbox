#include "ioj/sim/telemetry/level_telemetry_block_history.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <sandbox/core/diagnostics.h>

namespace ioj::sim {

namespace level_telemetry_block_history_detail {
auto capacity_for_bytes(std::size_t const bytes) -> std::int32_t {
    using Layout = ioj::sim::telemetry::HistoryRowsSingleLayout;
    auto low{std::size_t{0}};
    auto high{static_cast<std::size_t>(Layout::max_capacity / Layout::capacity_granularity)};
    while (low < high) {
        auto const middle{low + (high - low + 1) / 2};
        if (Layout::layout_bytes(middle) <= bytes) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }
    return static_cast<std::int32_t>(low * Layout::capacity_granularity);
}

constexpr auto logical_payload_bytes_per_row() -> std::size_t {
    return sizeof(std::uint64_t) * 5 + sizeof(ioj::sim::telemetry::HistoryFieldMask) +
           sizeof(std::int32_t) * 8 + sizeof(ioj::sim::telemetry::EntityTypeCounts) +
           sizeof(ioj::sim::telemetry::EntityCounts) + sizeof(double);
}
}

LevelTelemetryBlockHistory::Block::Block(GameMemoryBlock memory_block, std::int32_t const capacity)
    : memory_block{std::move(memory_block)}
    , storage{} {
    storage = {.data_ = this->memory_block.data(), .num_ = 0, .capacity_ = capacity};
}

LevelTelemetryBlockHistory::LevelTelemetryBlockHistory(GameMemory& memory,
                                                       LevelTelemetryHistoryConfig const config)
    : memory_{memory}
    , config_{config}
    , logical_payload_bytes_per_row_{level_telemetry_block_history_detail::
                                         logical_payload_bytes_per_row()}
    , rows_per_block_{
          level_telemetry_block_history_detail::capacity_for_bytes(config.block_bytes)} {
    using Layout = ioj::sim::telemetry::HistoryRowsSingleLayout;
    if (rows_per_block_ < Layout::capacity_granularity) {
        ml::fatal_error(
            std::format("Telemetry block budget of {} bytes cannot hold the minimum {} rows",
                        config_.block_bytes,
                        Layout::capacity_granularity));
    }
    layout_bytes_per_block_ = Layout::layout_bytes(
        static_cast<std::size_t>(rows_per_block_ / Layout::capacity_granularity));
    blocks_.reserve(64);
}

void LevelTelemetryBlockHistory::reset() {
    for (auto& block : blocks_) {
        block.storage.num_ = 0;
    }
    used_block_count_ = 0;
    row_count_ = 0;
}

auto LevelTelemetryBlockHistory::append_uninitialized()
    -> ioj::sim::telemetry::HistoryRowsSingleView {
    auto& block{acquire_or_reuse_next_block()};
    auto const row{block.storage.num_++};
    ++row_count_;
    return {&block.storage, row, 1};
}

auto LevelTelemetryBlockHistory::last_view() -> ioj::sim::telemetry::HistoryRowsSingleView {
    assert(row_count_ > 0 && used_block_count_ > 0);
    auto& storage{blocks_[used_block_count_ - 1].storage};
    return {&storage, storage.num_ - 1, 1};
}

auto LevelTelemetryBlockHistory::last_const_view() const
    -> ioj::sim::telemetry::HistoryRowsSingleConstView {
    assert(row_count_ > 0 && used_block_count_ > 0);
    auto const& storage{blocks_[used_block_count_ - 1].storage};
    return {&storage, storage.num_ - 1, 1};
}

auto LevelTelemetryBlockHistory::block_data(std::int32_t const index) const -> std::byte const* {
    assert(index >= 0 && static_cast<std::size_t>(index) < blocks_.size());
    return blocks_[index].memory_block.data();
}

auto LevelTelemetryBlockHistory::block_view(std::int32_t const index) const
    -> ioj::sim::telemetry::HistoryRowsSingleConstView {
    assert(index >= 0 && index < used_block_count_);
    auto const& storage{blocks_[index].storage};
    return {&storage, 0, storage.num_};
}

auto LevelTelemetryBlockHistory::get_stats() const noexcept -> LevelTelemetryBlockHistoryStats {
    auto const unused_samples{
        used_block_count_ > 0 ? rows_per_block_ - blocks_[used_block_count_ - 1].storage.num_ : 0};
    auto const overhead_per_block{layout_bytes_per_block_ -
                                  logical_payload_bytes_per_row_ * rows_per_block_};
    return {.configured_block_bytes = config_.block_bytes,
            .layout_bytes_per_block = layout_bytes_per_block_,
            .rows_per_block = rows_per_block_,
            .acquired_block_count = static_cast<std::int32_t>(blocks_.size()),
            .retained_block_count = static_cast<std::int32_t>(blocks_.size()),
            .peak_block_count = peak_block_count_,
            .total_sample_capacity = capacity(),
            .total_byte_capacity =
                layout_bytes_per_block_ * static_cast<std::int32_t>(blocks_.size()),
            .used_sample_count = row_count_,
            .used_payload_bytes = logical_payload_bytes_per_row_ * row_count_,
            .unused_samples_in_final_block = unused_samples,
            .unused_payload_bytes_in_final_block = logical_payload_bytes_per_row_ * unused_samples,
            .fixed_layout_overhead_bytes =
                overhead_per_block * static_cast<std::int32_t>(blocks_.size())};
}

auto LevelTelemetryBlockHistory::acquire_or_reuse_next_block() -> Block& {
    if (used_block_count_ > 0) {
        auto& current{blocks_[used_block_count_ - 1]};
        if (current.storage.num_ < rows_per_block_) {
            return current;
        }
    }

    if (used_block_count_ == static_cast<std::int32_t>(blocks_.size())) {
        using Layout = ioj::sim::telemetry::HistoryRowsSingleLayout;
        blocks_.emplace_back(
            memory_.acquire_block(layout_bytes_per_block_, Layout::allocation_alignment),
            rows_per_block_);
        peak_block_count_ = std::max(peak_block_count_, static_cast<std::int32_t>(blocks_.size()));
    }

    auto& next{blocks_[used_block_count_++]};
    assert(next.storage.num_ == 0);
    return next;
}
} // namespace ioj::sim
