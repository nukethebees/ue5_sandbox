#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
struct SourceDiskState {
    bool exists{};
    std::string digest{};
    auto operator==(SourceDiskState const&) const -> bool = default;
};

enum class PreviewState { None, Valid, Stale };

// Shared synchronization state; document lifetimes remain with the host.
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
    [[nodiscard]] auto save_replacement(std::string source, std::string_view synchronized_digest)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto save_replacement_as(std::string source,
                                           std::filesystem::path const& path,
                                           std::string_view synchronized_digest,
                                           SourceOverwritePolicy overwrite)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto refresh_external_conflict() -> std::expected<void, std::string>;

    [[nodiscard]] auto path() const -> std::filesystem::path const& { return path_; }
    [[nodiscard]] auto buffer() const -> std::string const& { return buffer_; }
    [[nodiscard]] auto is_attached() const -> bool { return attached_; }
    auto state() -> SourceSessionState& { return state_; }
    auto state() const -> SourceSessionState const& { return state_; }
    [[nodiscard]] static auto source_digest(std::string_view source) -> std::string;
  private:
    [[nodiscard]] auto load_attached(std::filesystem::path const& path)
        -> std::expected<void, std::string>;
    [[nodiscard]] auto write(std::string const& source,
                             std::filesystem::path const& path,
                             SourceOverwritePolicy overwrite,
                             bool replace_buffer) -> std::expected<void, std::string>;

    std::filesystem::path path_{};
    std::string buffer_{};
    SourceSessionState state_{};
    bool attached_{};
};
}
