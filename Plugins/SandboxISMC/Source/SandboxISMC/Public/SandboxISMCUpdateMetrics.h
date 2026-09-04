#pragma once

#include "CoreTypes.h"

struct SANDBOXISMC_API FSandboxISMCUpdateMetrics {
    int32 instance_count{0};
    double prepare_ms{0.0};
    double pack_ms{0.0};
    double bounds_ms{0.0};
    double submit_ms{0.0};
    double upload_ms{0.0};
    uint64 upload_bytes{0};
    int32 dirty_instance_count{0};
    int32 dirty_range_count{0};
};
