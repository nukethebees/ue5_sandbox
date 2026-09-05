#pragma once

#include "Math/Vector4.h"

struct SANDBOXISMC_API FSandboxISMCRenderInstance {
    FVector4f origin;
    FVector4f transform_row_0;
    FVector4f transform_row_1;
    FVector4f transform_row_2;
};

static_assert(sizeof(FSandboxISMCRenderInstance) == 64);
