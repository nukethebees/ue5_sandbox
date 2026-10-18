#include <ioj/levels/authoring/source_session.h>

#include <utility>

namespace ioj::levels::authoring {
auto SourceSessionState::unapplied(std::string_view const synchronized_digest) const -> bool {
    return dirty() || (!buffer_empty && buffer_digest != synchronized_digest);
}
void SourceSessionState::document_changed() {
    recovery_required = recovery_required || dirty();
    ++revision;
    scene_dirty = false;
    preview = PreviewState::None;
}
void SourceSessionState::loaded(SourceDiskState disk,
                                std::string digest,
                                bool const empty,
                                bool const source_changed) {
    disk_baseline = std::move(disk);
    buffer_digest = std::move(digest);
    saved_buffer_digest = buffer_digest;
    buffer_empty = empty;
    disk_conflict = false;
    recovery_required = false;
    if (source_changed) {
        ++revision;
        invalidate_preview();
    }
}
void SourceSessionState::edited(std::string digest, bool const empty) {
    if (buffer_digest == digest) {
        return;
    }
    buffer_digest = std::move(digest);
    buffer_empty = empty;
    ++revision;
    invalidate_preview();
}
void SourceSessionState::saved(std::string digest, bool const source_changed) {
    disk_baseline = {.exists = true, .digest = digest};
    saved_buffer_digest = std::move(digest);
    disk_conflict = false;
    if (source_changed) {
        ++revision;
        invalidate_preview();
    }
}
void SourceSessionState::observe(SourceDiskState const& disk) {
    disk_conflict = disk != disk_baseline;
}
void SourceSessionState::invalidate_preview() {
    if (preview == PreviewState::Valid) {
        preview = PreviewState::Stale;
    }
}
}
