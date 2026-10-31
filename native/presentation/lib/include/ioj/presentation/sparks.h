#pragma once

#include <ioj/presentation/spark_burst.h>
#include <ioj/presentation/spark_particle_record.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::presentation {
auto spark_particle_count(SparkBurst const& burst) -> std::int32_t;
auto expand_spark_bursts(std::span<SparkBurst const> bursts,
                         std::int32_t capacity,
                         float effect_time,
                         std::vector<SparkParticleRecord>& output) -> std::int64_t;
}
