#pragma once

#include <ioj/levels/authoring/source_overwrite_policy.h>
#include <ioj/levels/authoring/source_replay.h>
#include <ioj/levels/authoring/source_session_state.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace ioj::levels::authoring {
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

    [[nodiscard]] auto read(settings::Settings base = {},
                            std::filesystem::path const& library_root = {}) const
        -> std::expected<LevelDefinition, Diagnostics>;
    [[nodiscard]] auto read_replay(settings::Settings base = {},
                                   std::filesystem::path const& library_root = {}) const
        -> std::expected<SourceReplay, Diagnostics>;

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
