#pragma once

#include "SandboxISMCStagingBuffer.h"

#include "SandboxCore/multi_buffer.h"

struct FSandboxISMCStagingState {
    ml::MultiBuffer<FSandboxISMCStagingBuffer, 3> buffers;
};
