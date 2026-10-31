#pragma once

#include <ioj/presentation/spark_particle_record.h>

#include "Containers/Array.h"
#include "Templates/Atomic.h"

struct FSparkUploadBuffer {
    TArray<ioj::presentation::SparkParticleRecord> particles;
    TArray<int32> destinations;
    TArray<int32> counts;
    TAtomic<bool> in_flight{false};
};
