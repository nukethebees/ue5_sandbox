#include "SandboxUI/Radar/RadarFrameStore.h"

auto FRadarFrameStore::next() -> FRadarFrame& {
    return frames_.next();
}

void FRadarFrameStore::publish() {
    frames_.cycle();
}

auto FRadarFrameStore::current() const -> FRadarFrame const& {
    return frames_.current();
}
