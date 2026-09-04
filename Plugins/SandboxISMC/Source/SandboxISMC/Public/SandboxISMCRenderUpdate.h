#pragma once

#include "SandboxISMCRenderInstance.h"
#include "SandboxISMCRenderRange.h"

#include "Containers/Array.h"

struct SANDBOXISMC_API FSandboxISMCRenderUpdate {
    int32 instance_count{0};
    bool full_upload{false};
    TArray<FSandboxISMCRenderRange> ranges;
    TArray<FSandboxISMCRenderInstance> instances;
};
