#pragma once

#include <ioj/presentation/spark_burst.h>
#include <ioj/presentation/spark_gpu_batch.h>
#include <ioj/presentation/spark_particle_record.h>
#include <ioj/presentation/spark_particle_submission.h>
#include <ioj/presentation/spark_submission_result.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::presentation {
class SparkStorage {
  public:
    // Require a nonnegative capacity. Reset discards pending particle lifetimes.
    void reset(std::int32_t capacity);
    void clear();
    auto capacity() const -> std::int32_t;
    auto particles() const -> std::span<SparkParticleRecord const>;
    auto latest_expiry_time() const -> float;
    auto append_bursts(std::span<SparkBurst const> bursts,
                       float effect_time,
                       SparkGpuBatch& pending) -> SparkSubmissionResult;
    auto submit_particles(std::span<SparkParticleRecord const> particles, float effect_time)
        -> SparkParticleSubmission;
  private:
    std::vector<SparkParticleRecord> particles_;
    std::int32_t cursor_{};
    std::int32_t allocated_slots_{};
    float latest_expiry_time_{};
};
}
