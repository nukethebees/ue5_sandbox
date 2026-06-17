#pragma once

#include <HAL/Platform.h>

enum class ERegistryHandleState : uint8 {
    Active,
    Stale,
    Invalid,
};
