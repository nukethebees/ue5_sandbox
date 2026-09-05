#pragma once

#include "CoreTypes.h"

struct SANDBOXISMC_API FSandboxISMCUpdateMetrics {
    int32 instance_count{0};
    double build_ms{0.0};
    double submit_ms{0.0};
    double upload_ms{0.0};
    uint64 transform_upload_bytes{0};
    uint64 custom_data_upload_bytes{0};
    uint64 upload_bytes{0};
    // Lifetime totals; render-thread counters can lag the latest submitted snapshot.
    uint64 staging_capacity_changes{0};
    uint64 gpu_buffer_allocations{0};
    uint64 staging_waits{0};
    double staging_wait_ms{0.0};
};
