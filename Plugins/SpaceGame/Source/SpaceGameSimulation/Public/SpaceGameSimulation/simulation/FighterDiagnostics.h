#pragma once

#include <HAL/IConsoleManager.h>

namespace ml::fighter_diagnostics {
inline TAutoConsoleVariable<int32> enabled{
    TEXT("sg.FighterDiagnostics"),
    0,
    TEXT("Log bounded fighter spawn and blocked-navigation reports. Toggle off/on to rearm.")};

inline auto take_report(int32& emitted, int32 const limit) -> bool {
    if (enabled.GetValueOnGameThread() == 0) {
        emitted = 0;
        return false;
    }
    if (emitted >= limit) {
        return false;
    }
    ++emitted;
    return true;
}
}
