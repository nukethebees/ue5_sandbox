#include "SandboxUI/EntityOverlay/EntityOverlayFrameStore.h"

auto FEntityOverlayFrameStore::next() -> FEntityOverlayFrame& {
    return frames_.next();
}

void FEntityOverlayFrameStore::publish() {
    frames_.cycle();
}

auto FEntityOverlayFrameStore::current() const -> FEntityOverlayFrame const& {
    return frames_.current();
}
