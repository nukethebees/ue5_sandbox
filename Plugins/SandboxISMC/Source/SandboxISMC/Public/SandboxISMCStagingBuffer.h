#pragma once

#include "SandboxISMCRenderInstance.h"

#include "Containers/Array.h"
#include "Templates/Atomic.h"

struct SANDBOXISMC_API FSandboxISMCStagingBuffer {
    TArray<FSandboxISMCRenderInstance> instances;
    TAtomic<bool> in_flight{false};
};
