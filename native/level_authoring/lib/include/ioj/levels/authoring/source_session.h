#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
struct SourceDiskState {
    auto operator==(SourceDiskState const&) const -> bool = default;

    bool exists{};
    std::string source{};
};

enum class PreviewState { None, Valid, Stale };

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

enum class SourceOverwritePolicy { RefuseExisting, ReplaceExisting };

// Own UTF-8 source independently of the editor document. Detach before attaching
// another document, even when both documents use the same path.
class SourceSession {
  public:
    [[nodiscard]] auto attach(std::filesystem::path const& path)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto load(std::filesystem::path const& path) -> std::expected<void, std::string>;
    [[nodiscard]] auto reload() -> std::expected<void, std::string>;
    [[nodiscard]] auto discard_and_attach(std::filesystem::path const& path)
        -> std::expected<void, std::string>;
    void detach();
    void set_buffer(std::string source);

    [[nodiscard]] auto save() -> std::expected<void, std::string>;
    [[nodiscard]] auto save_as(std::filesystem::path const& path, SourceOverwritePolicy overwrite)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto save_replacement(std::string const& source,
                                        std::string_view synchronized_source)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto save_replacement_as(std::string const& source,
                                           std::filesystem::path const& path,
                                           std::string_view synchronized_source,
                                           SourceOverwritePolicy overwrite)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto refresh_external_conflict() -> std::expected<void, std::string>;

    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
    [[nodiscard]] auto buffer() const -> std::string const& { return state_.buffer_text; }
    [[nodiscard]] auto is_attached() const -> bool { return attached_; }
    auto state() -> SourceSessionState& { return state_; }
    auto state() const -> SourceSessionState const& { return state_; }
  private:
    [[nodiscard]] auto load_attached(std::filesystem::path const& path)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto write(std::string const& source,
                             std::filesystem::path const& path,
                             SourceOverwritePolicy overwrite,
                             bool replace_buffer) -> std::expected<void, std::string>;

    std::filesystem::path path_{};
    SourceSessionState state_{};
    bool attached_{};
};
}
