#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
struct SourceDiskState {
    bool exists{};
    std::string digest{};
    auto operator==(SourceDiskState const&) const -> bool = default;
};

enum class PreviewState { None, Valid, Stale };

// Pure bookkeeping. The host owns text, filesystem operations, and document lifetimes.
struct SourceSessionState {
    SourceDiskState disk_baseline{};
    std::string buffer_digest{};
    std::string saved_buffer_digest{};
    std::uint64_t revision{};
    bool buffer_empty{true};
    bool disk_conflict{};
    bool scene_dirty{};
    bool recovery_required{};
    PreviewState preview{PreviewState::None};

    [[nodiscard]] auto dirty() const -> bool { return buffer_digest != saved_buffer_digest; }
    [[nodiscard]] auto unapplied(std::string_view synchronized_digest) const -> bool;
    void document_changed();
    void loaded(SourceDiskState disk, std::string digest, bool empty, bool source_changed);
    void edited(std::string digest, bool empty);
    void saved(std::string digest, bool source_changed);
    void observe(SourceDiskState const& disk);
    void invalidate_preview();
};
}
