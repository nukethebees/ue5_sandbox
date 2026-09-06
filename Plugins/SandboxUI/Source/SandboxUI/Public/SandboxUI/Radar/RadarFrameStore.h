#pragma once

#include "SandboxUI/Radar/RadarTypes.h"

#include "SandboxCore/multi_buffer.h"
#include "Templates/SharedPointer.h"

class SANDBOXUI_API FRadarFrameStore final {
  public:
    static constexpr int32 buffer_count{3};

    [[nodiscard]] auto next() -> FRadarFrame&;
    void publish();
    [[nodiscard]] auto current() const -> FRadarFrame const&;
  private:
    ml::MultiBuffer<FRadarFrame, buffer_count> frames_;
};

using FRadarFrameStorePtr = TSharedPtr<FRadarFrameStore, ESPMode::ThreadSafe>;
using FRadarFrameStoreConstPtr = TSharedPtr<FRadarFrameStore const, ESPMode::ThreadSafe>;
