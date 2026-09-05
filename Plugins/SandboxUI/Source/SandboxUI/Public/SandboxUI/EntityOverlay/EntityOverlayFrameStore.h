#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include "SandboxCore/multi_buffer.h"
#include "Templates/SharedPointer.h"

class SANDBOXUI_API FEntityOverlayFrameStore final {
  public:
    static constexpr int32 buffer_count{3};

    [[nodiscard]] auto next() -> FEntityOverlayFrame&;
    void publish();
    [[nodiscard]] auto current() const -> FEntityOverlayFrame const&;
  private:
    ml::MultiBuffer<FEntityOverlayFrame, buffer_count> frames_;
};

using FEntityOverlayFrameStorePtr = TSharedPtr<FEntityOverlayFrameStore, ESPMode::ThreadSafe>;
using FEntityOverlayFrameStoreConstPtr =
    TSharedPtr<FEntityOverlayFrameStore const, ESPMode::ThreadSafe>;
