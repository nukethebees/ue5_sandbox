#pragma once

#include "CoreTypes.h"

struct SANDBOXISMC_API FSandboxISMCUpdateMetrics {
    int32 instance_count{0};
    double build_ms{0.0};  // Producer packing and bounds reduction, excluding staging acquisition.
    double submit_ms{0.0}; // Game-thread render-command enqueue CPU time.
    double upload_ms{0.0}; // Render-thread allocation/lock/copy/unlock CPU time, not GPU time.
    uint64 transform_submitted_bytes{0};
    uint64 custom_data_submitted_bytes{0};
    uint64 submitted_bytes{0};
    uint64 uploaded_bytes{0}; // Last consumed snapshot; may lag or skip producer snapshots.
    uint64 total_uploaded_bytes{0};
    uint64 uploads{0};
    // Lifetime totals; render-thread counters can lag the latest submitted snapshot.
    uint64 staging_capacity_changes{0};
    uint64 gpu_buffer_allocations{0};
    uint64 staging_waits{0};
    double staging_wait_ms{0.0};
};
