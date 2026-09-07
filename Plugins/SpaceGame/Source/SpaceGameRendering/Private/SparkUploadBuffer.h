#pragma once

#include "SpaceGameRendering/SparkParticleRecord.h"

#include "Containers/Array.h"
#include "Templates/Atomic.h"

struct FSparkUploadBuffer {
    TArray<FSparkParticleRecord> particles;
    TArray<int32> destinations;
    TArray<int32> counts;
    TAtomic<bool> in_flight{false};
};
