#pragma once

#include <ioj/levels/authoring/preview_state.h>
#include <ioj/levels/authoring/source_disk_state.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
// Shared synchronization state; document lifetimes remain with the host.
struct SourceSessionState {
    [[nodiscard]] auto dirty() const -> bool { return buffer_text != saved_buffer_text; }
    [[nodiscard]] auto unapplied(std::string_view synchronized_source) const -> bool;
    void document_changed();
    void loaded(SourceDiskState disk, std::string source, bool source_changed);
    void edited(std::string source);
    void saved(bool source_changed);
    void observe(SourceDiskState const& disk);
    void invalidate_preview();

    SourceDiskState disk_baseline{};
    std::string buffer_text{};
    std::string saved_buffer_text{};
    std::uint64_t revision{};
    bool disk_conflict{};
    bool scene_dirty{};
    bool recovery_required{};
    PreviewState preview{PreviewState::None};
};
}
