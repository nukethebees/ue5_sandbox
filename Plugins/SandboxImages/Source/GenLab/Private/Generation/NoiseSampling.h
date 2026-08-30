#pragma once

#include "CoreMinimal.h"

namespace SandboxImages::GenLab::NoiseSampling {

class FDeterministicRandom {
  public:
    explicit FDeterministicRandom(uint32 seed);

    auto next_unit() -> float;

  private:
    uint32 state_;
};

auto lattice_value(int32 x, int32 y, uint32 seed) -> float;
auto fractal_noise_sample(float x,
                          float y,
                          int32 width,
                          int32 height,
                          uint32 seed,
                          float base_scale,
                          int32 octave_count,
                          float persistence,
                          bool tileable) -> float;

}
