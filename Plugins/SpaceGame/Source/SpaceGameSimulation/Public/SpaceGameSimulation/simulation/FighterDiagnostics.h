#pragma once

#include <HAL/IConsoleManager.h>

namespace ml::fighter_diagnostics {
inline TAutoConsoleVariable<int32> enabled{
    TEXT("sg.FighterDiagnostics"),
    0,
    TEXT("Log bounded fighter spawn and blocked-navigation reports. Toggle off/on to rearm.")};

}
