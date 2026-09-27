#pragma once

#include "SandboxISMCRenderInstance.h"

#include "Containers/Array.h"
#include "Math/Vector4.h"
#include "Templates/Atomic.h"

struct SANDBOXISMC_API FSandboxISMCStagingBuffer {
    TArray<FSandboxISMCRenderInstance> instances;
    TArray<float> custom_data;
    FVector4f position_root_quantum{0.0f, 0.0f, 0.0f, ml::sandbox_ismc::position_quantum};
    int32 num_custom_data_floats{0};
    TAtomic<bool> in_flight{false};
};
