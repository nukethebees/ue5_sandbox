#pragma once

#include <ioj/presentation/gpu_spark_burst.h>

#include <cstddef>
#include <vector>

namespace ioj::presentation {
struct SparkGpuBatch {
    void clear();
    auto upload_bytes() const -> std::size_t;

    std::vector<GpuSparkBurst> bursts;
    std::vector<std::uint32_t> selection_indices;
};
}
