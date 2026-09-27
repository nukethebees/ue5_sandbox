#pragma once

#include "sandbox/core/sandbox_ismc_transform.h"

using FSandboxISMCRenderInstance = ml::sandbox_ismc::PackedTransform;

static_assert(sizeof(FSandboxISMCRenderInstance) == 16);
