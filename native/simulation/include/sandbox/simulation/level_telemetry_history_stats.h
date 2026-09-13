#pragma once

#include <cstddef>
#include <cstdint>

namespace ml::simulation {
struct LevelTelemetryHistoryStats {
    std::size_t configured_block_bytes{};
    std::size_t layout_bytes_per_block{};
    std::int32_t rows_per_block{};
    std::int32_t acquired_block_count{};
    std::int32_t retained_block_count{};
    std::int32_t peak_block_count{};
    std::int32_t total_sample_capacity{};
    std::size_t total_byte_capacity{};
    std::int32_t used_sample_count{};
    std::size_t used_payload_bytes{};
    std::int32_t unused_samples_in_final_block{};
    std::size_t unused_payload_bytes_in_final_block{};
    std::size_t fixed_layout_overhead_bytes{};
    std::uint64_t payload_write_count{};
};
} // namespace ml::simulation
