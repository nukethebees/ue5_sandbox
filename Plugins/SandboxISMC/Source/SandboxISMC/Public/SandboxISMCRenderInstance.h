#pragma once

#include "ioj/ismc/transform.h"

using FSandboxISMCRenderInstance = ioj::ismc::PackedTransform;

static_assert(sizeof(FSandboxISMCRenderInstance) == 12);
