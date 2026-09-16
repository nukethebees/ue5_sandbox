#pragma once

#include "SparkUploadBuffer.h"

#include "sandbox/core/multi_buffer.h"

struct FSparkStagingState {
    ml::MultiBuffer<FSparkUploadBuffer, 3> buffers;
};
