#pragma once

#include "ioj/multi_buffer.h"
#include "SandboxISMCStagingBuffer.h"

struct FSandboxISMCStagingState {
    ioj::MultiBuffer<FSandboxISMCStagingBuffer, 3> buffers;
};
