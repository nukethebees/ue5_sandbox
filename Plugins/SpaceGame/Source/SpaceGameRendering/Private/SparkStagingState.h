#pragma once

#include "ioj/multi_buffer.h"
#include "SparkUploadBuffer.h"

struct FSparkStagingState {
    ioj::MultiBuffer<FSparkUploadBuffer, 3> buffers;
};
