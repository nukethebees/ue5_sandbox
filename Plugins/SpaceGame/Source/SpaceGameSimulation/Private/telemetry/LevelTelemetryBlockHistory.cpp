#include "SpaceGameSimulation/telemetry/LevelTelemetryBlockHistory.h"

namespace level_telemetry_block_history_detail {
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

constexpr auto logical_payload_bytes_per_row() -> SIZE_T {
    return sizeof(uint64) * 6 + sizeof(int32) * 8 + sizeof(FTestEntityRegistry::EntityTypeCounts) +
           sizeof(FTestEntityRegistry::EntityCounts) + sizeof(double);
}
}

FLevelTelemetryBlockHistory::FBlock::FBlock(FGameMemoryBlock memory_block, int32 const capacity)
    : memory_block{MoveTemp(memory_block)}
    , storage{.data_ = this->memory_block.data(), .num_ = 0, .capacity_ = capacity} {}

FLevelTelemetryBlockHistory::FLevelTelemetryBlockHistory(FGameMemory& memory,
                                                         FLevelTelemetryHistoryConfig const config)
    : memory_{memory}
    , config_{config}
    , logical_payload_bytes_per_row_{level_telemetry_block_history_detail::
                                         logical_payload_bytes_per_row()}
    , rows_per_block_{
          level_telemetry_block_history_detail::capacity_for_bytes(config.block_bytes)} {
    using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
    checkf(rows_per_block_ >= Layout::capacity_granularity,
           TEXT("Telemetry block budget of %llu bytes cannot hold the minimum %d rows."),
           static_cast<uint64>(config_.block_bytes),
           Layout::capacity_granularity);
    layout_bytes_per_block_ =
        Layout::layout_bytes(static_cast<SIZE_T>(rows_per_block_ / Layout::capacity_granularity));
    blocks_.Reserve(64);
}

void FLevelTelemetryBlockHistory::reset() {
    for (auto& block : blocks_) {
        block.storage.num_ = 0;
    }
    used_block_count_ = 0;
    row_count_ = 0;
}

auto FLevelTelemetryBlockHistory::append_uninitialized()
    -> ml::level_telemetry::FHistoryRowsSingleView {
    auto& block{acquire_or_reuse_next_block()};
    auto const row{block.storage.num_++};
    ++row_count_;
    return {&block.storage, row, 1};
}

auto FLevelTelemetryBlockHistory::last_view() -> ml::level_telemetry::FHistoryRowsSingleView {
    check(row_count_ > 0 && used_block_count_ > 0);
    auto& storage{blocks_[used_block_count_ - 1].storage};
    return {&storage, storage.num_ - 1, 1};
}

auto FLevelTelemetryBlockHistory::last_const_view() const
    -> ml::level_telemetry::FHistoryRowsSingleConstView {
    check(row_count_ > 0 && used_block_count_ > 0);
    auto const& storage{blocks_[used_block_count_ - 1].storage};
    return {&storage, storage.num_ - 1, 1};
}

auto FLevelTelemetryBlockHistory::block_data(int32 const index) const -> std::byte const* {
    check(blocks_.IsValidIndex(index));
    return blocks_[index].memory_block.data();
}

auto FLevelTelemetryBlockHistory::block_view(int32 const index) const
    -> ml::level_telemetry::FHistoryRowsSingleConstView {
    check(index >= 0 && index < used_block_count_);
    auto const& storage{blocks_[index].storage};
    return {&storage, 0, storage.num_};
}

auto FLevelTelemetryBlockHistory::get_stats() const noexcept -> FLevelTelemetryBlockHistoryStats {
    auto const unused_samples{
        used_block_count_ > 0 ? rows_per_block_ - blocks_[used_block_count_ - 1].storage.num_ : 0};
    auto const overhead_per_block{layout_bytes_per_block_ -
                                  logical_payload_bytes_per_row_ * rows_per_block_};
    return {.configured_block_bytes = config_.block_bytes,
            .layout_bytes_per_block = layout_bytes_per_block_,
            .rows_per_block = rows_per_block_,
            .acquired_block_count = blocks_.Num(),
            .retained_block_count = blocks_.Num(),
            .peak_block_count = peak_block_count_,
            .total_sample_capacity = capacity(),
            .total_byte_capacity = layout_bytes_per_block_ * blocks_.Num(),
            .used_sample_count = row_count_,
            .used_payload_bytes = logical_payload_bytes_per_row_ * row_count_,
            .unused_samples_in_final_block = unused_samples,
            .unused_payload_bytes_in_final_block = logical_payload_bytes_per_row_ * unused_samples,
            .fixed_layout_overhead_bytes = overhead_per_block * blocks_.Num()};
}

auto FLevelTelemetryBlockHistory::acquire_or_reuse_next_block() -> FBlock& {
    if (used_block_count_ > 0) {
        auto& current{blocks_[used_block_count_ - 1]};
        if (current.storage.num_ < rows_per_block_) {
            return current;
        }
    }

    if (used_block_count_ == blocks_.Num()) {
        using Layout = ml::level_telemetry::FHistoryRowsSingleLayout;
        blocks_.Emplace(
            memory_.acquire_block(layout_bytes_per_block_, Layout::allocation_alignment),
            rows_per_block_);
        peak_block_count_ = FMath::Max(peak_block_count_, blocks_.Num());
    }

    auto& next{blocks_[used_block_count_++]};
    check(next.storage.num_ == 0);
    return next;
}
