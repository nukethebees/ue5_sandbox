#pragma once

#include "GpuSparkBurst.h"

#include "Containers/Array.h"

struct FSparkPendingGpuBatches {
    TArray<FGpuSparkBurst> bursts;
    TArray<uint32> selection_indices;
};
