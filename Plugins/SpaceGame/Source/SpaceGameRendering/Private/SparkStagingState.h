#pragma once

#include "ioj/multi_buffer.h"
#include "SparkUploadBuffer.h"

struct FSparkStagingState {
    ml::MultiBuffer<FSparkUploadBuffer, 3> buffers;
};
