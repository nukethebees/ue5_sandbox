#include "Sandbox/logging/FrameLogTracker.h"

int32 FFrameLogTracker::get_and_reset_log_count() {
    // Atomically read and reset the counter to ensure thread safety,
    // although in this specific use case it will be called from the game thread.
    int32 const captured_count{FPlatformAtomics::InterlockedExchange(&log_count_this_frame, 0)};
    return captured_count;
}

void FFrameLogTracker::Serialize(const TCHAR* V,
                                 ELogVerbosity::Type Verbosity,
                                 FName const& Category) {
    static FName const to_ignore{TEXT("LogSandboxFrameCount")};
    if (Category != to_ignore) {
        FPlatformAtomics::InterlockedIncrement(&log_count_this_frame);
    }
}
