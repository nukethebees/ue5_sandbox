#pragma once

#include "SandboxISMCRenderUpdate.h"

#include "Templates/SharedPointer.h"

struct SANDBOXISMC_API FSandboxISMCPreparedUpdate {
    TSharedPtr<FSandboxISMCRenderUpdate, ESPMode::ThreadSafe> render_update;
    uint64 pack_cycles{0};
    uint64 bounds_cycles{0};
    int32 dirty_instance_count{0};
    int32 dirty_range_count{0};
};
