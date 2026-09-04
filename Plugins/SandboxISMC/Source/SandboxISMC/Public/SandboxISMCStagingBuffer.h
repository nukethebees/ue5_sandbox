#pragma once

#include "SandboxISMCRenderInstance.h"

#include "Containers/Array.h"
#include "Templates/Atomic.h"

struct SANDBOXISMC_API FSandboxISMCStagingBuffer {
    TArray<FSandboxISMCRenderInstance> instances;
    TArray<float> custom_data;
    int32 num_custom_data_floats{0};
    TAtomic<bool> in_flight{false};
};
