#pragma once

#include <ioj/presentation/spark_upload_range.h>

#include <array>
#include <cstdint>

namespace ioj::presentation {
struct SparkParticleSubmission {
    std::array<SparkUploadRange, 2> ranges{};
    std::int32_t range_count{};
    std::int32_t overwritten{};
};
}
