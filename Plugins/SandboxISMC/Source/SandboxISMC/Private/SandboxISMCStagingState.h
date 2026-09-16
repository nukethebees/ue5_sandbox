#pragma once

#include "SandboxISMCStagingBuffer.h"

#include "sandbox/core/multi_buffer.h"

struct FSandboxISMCStagingState {
    ml::MultiBuffer<FSandboxISMCStagingBuffer, 3> buffers;
};
