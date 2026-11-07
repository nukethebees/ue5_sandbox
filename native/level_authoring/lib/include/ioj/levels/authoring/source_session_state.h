#pragma once

#include <ioj/levels/authoring/source_disk_state.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
struct SourceSessionState {
    [[nodiscard]] auto dirty() const -> bool { return buffer_text != saved_buffer_text; }
    void document_changed();
    void loaded(SourceDiskState disk, std::string source, bool source_changed);
    void edited(std::string source);
    void saved();
    void observe(SourceDiskState const& disk);

    SourceDiskState disk_baseline{};
    std::string buffer_text{};
    std::string saved_buffer_text{};
    std::uint64_t revision{};
    bool disk_conflict{};
    bool recovery_required{};
};
}
