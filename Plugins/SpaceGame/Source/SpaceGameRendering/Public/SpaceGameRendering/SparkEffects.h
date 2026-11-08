#pragma once

#include <ioj/presentation/spark_particle_record.h>
#include <ioj/presentation/spark_queue.h>

namespace ioj {
class USparkRendererComponent;
} // namespace ioj

class SPACEGAMERENDERING_API FSparkEffects {
  public:
    explicit FSparkEffects(ioj::USparkRendererComponent& renderer);

    void queue_burst(ioj::presentation::SparkBurst const& burst);
    void commit(float dt);
    void clear();
  private:
    ioj::USparkRendererComponent* renderer_;
    ioj::presentation::SparkQueue queue_;
    std::vector<ioj::presentation::SparkParticleRecord> expanded_particles_;
};
