#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * An output device that tracks if any logs have been made this frame.
 */
class FFrameLogTracker : public FOutputDevice {
  public:
    int32 get_and_reset_log_count();
  protected:
    virtual void
        Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, FName const& Category) override;
  private:
    int32 log_count_this_frame{0};
};
