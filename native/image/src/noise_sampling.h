#pragma once

#include <cstdint>

namespace sandbox::image::noise {

class DeterministicRandom {
  public:
    explicit DeterministicRandom(std::uint32_t seed);

    auto next_unit() -> float;
  private:
    std::uint32_t state_;
};

auto lattice_value(std::int32_t x, std::int32_t y, std::uint32_t seed) -> float;
auto fractal_noise_sample(float x,
                          float y,
                          std::int32_t width,
                          std::int32_t height,
                          std::uint32_t seed,
                          float base_scale,
                          std::int32_t octave_count,
                          float persistence,
                          bool tileable) -> float;

}
