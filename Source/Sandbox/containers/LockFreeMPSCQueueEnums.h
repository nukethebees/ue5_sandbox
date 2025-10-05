#pragma once

#include <cstdint>

namespace ml {
enum class ELockFreeMPSCQueueInitResult : std::uint8_t {
    Success,
    AlreadyInitialised,
    AllocationFailed
};
enum class ELockFreeMPSCQueueEnqueueResult : std::uint8_t { Success, Full, Uninitialised };
}
