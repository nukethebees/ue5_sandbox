#pragma once

#include <CoreMinimal.h>
#include <SpaceGameRendering/SparkBurst.h>
#include <SpaceGameRendering/SparkParticleRecord.h>

class USparkRendererComponent;

auto SPACEGAMERENDERING_API expand_spark_bursts(TConstArrayView<FSparkBurst> bursts,
                                                int32 capacity,
                                                float effect_time,
                                                TArray<FSparkParticleRecord>& output) -> int64;

class SPACEGAMERENDERING_API FSparkEffects {
  public:
    explicit FSparkEffects(USparkRendererComponent& renderer);

    void queue_burst(FSparkBurst const& burst);
    void commit(float dt);
    void clear();
  private:
    USparkRendererComponent* renderer_{nullptr};
    TArray<FSparkBurst> queued_bursts_;
    TArray<FSparkParticleRecord> expanded_particles_;
    float effect_time_{0.0f};
    int64 dropped_bursts_{0};
};
