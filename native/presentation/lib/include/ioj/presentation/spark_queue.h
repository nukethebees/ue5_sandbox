#pragma once

#include <ioj/presentation/spark_burst.h>

#include <cstdint>
#include <span>
#include <vector>

namespace ioj::presentation {
class SparkQueue {
  public:
    explicit SparkQueue(std::int32_t capacity, float effect_time = 0.f);
    void queue(SparkBurst const& burst);
    void advance(float delta_seconds);
    void clear_pending();
    void clear();
    auto bursts() -> std::span<SparkBurst>;
    auto effect_time() const -> float;
    auto dropped_bursts() const -> std::int64_t;
  private:
    std::vector<SparkBurst> bursts_;
    std::int32_t capacity_{};
    float effect_time_{};
    std::int64_t dropped_bursts_{};
};
}
