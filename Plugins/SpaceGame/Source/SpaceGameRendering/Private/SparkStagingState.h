#pragma once

#include "SparkUploadBuffer.h"

#include "SandboxCore/multi_buffer.h"

struct FSparkStagingState {
    ml::MultiBuffer<FSparkUploadBuffer, 3> buffers;
};
