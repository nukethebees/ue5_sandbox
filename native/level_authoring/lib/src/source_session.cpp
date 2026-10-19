#include <ioj/levels/authoring/source_session.h>

#include <ioj/ascii.h>
#include <ioj/files.h>

#include <format>
#include <fstream>
#include <utility>

namespace ioj::levels::authoring {
namespace source_session_detail {
auto normalize(std::filesystem::path const& path) -> std::filesystem::path {
    return path.empty() ? path : std::filesystem::absolute(path).lexically_normal();
}

auto read_source(std::filesystem::path const& path) -> std::expected<std::string, std::string> {
    auto bytes{read_file(path)};
    if (!bytes) {
        return std::unexpected{std::format("Could not read source file '{}'.", path_to_utf8(path))};
    }
    if (bytes->starts_with("\xff\xfe") || bytes->starts_with("\xfe\xff")) {
        return std::unexpected{std::format(
            "Source file '{}' is UTF-16. Save it as UTF-8 before opening it.", path_to_utf8(path))};
    }
    if (bytes->starts_with("\xef\xbb\xbf")) {
        bytes->erase(0, 3);
    }
    return std::move(*bytes);
}

auto observe(std::filesystem::path const& path) -> std::expected<SourceDiskState, std::string> {
    std::error_code error;
    auto const exists{!path.empty() && std::filesystem::exists(path, error)};
    if (error) {
        return std::unexpected{std::format(
            "Could not inspect source file '{}': {}", path_to_utf8(path), error.message())};
    }
    if (!exists) {
        return SourceDiskState{};
    }
    auto source{read_source(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    return SourceDiskState{true, std::move(*source)};
}
}

auto SourceSession::attach(std::filesystem::path const& path) -> std::expected<void, std::string> {
    if (attached_ && path_ == source_session_detail::normalize(path)) {
        return {};
    }
    if (state_.dirty() || state_.recovery_required) {
        detach();
        return std::unexpected{
            "The source buffer has unsaved changes and was detached from the new document."};
    }
    auto result{load_attached(path)};
    if (!result) {
        detach();
        state_.disk_conflict = true;
    }
    return result;
}

auto SourceSession::load(std::filesystem::path const& path) -> std::expected<void, std::string> {
    if (state_.dirty() || state_.recovery_required) {
        return std::unexpected{"Save or discard the edited source buffer before loading."};
    }
    std::error_code error;
    if (path.empty() || !std::filesystem::exists(path, error) || error) {
        return std::unexpected{std::format("Could not read source file '{}'.", path_to_utf8(path))};
    }
    return load_attached(path);
}

auto SourceSession::reload() -> std::expected<void, std::string> {
    if (!attached_) {
        return std::unexpected{"The source buffer is not attached to a level document."};
    }
    return load_attached(path_);
}

auto SourceSession::discard_and_attach(std::filesystem::path const& path)
    -> std::expected<void, std::string> {
    return load_attached(path);
}

void SourceSession::detach() {
    if (attached_ || state_.scene_dirty || state_.preview != PreviewState::None ||
        (state_.dirty() && !state_.recovery_required)) {
        state_.document_changed();
    }
    attached_ = false;
}

void SourceSession::set_buffer(std::string source) {
    if (source == state_.buffer_text) {
        return;
    }
    state_.edited(std::move(source));
}

auto SourceSession::load_attached(std::filesystem::path const& path)
    -> std::expected<void, std::string> {
    auto normalized{source_session_detail::normalize(path)};
    auto disk{source_session_detail::observe(normalized)};
    if (!disk) {
        return std::unexpected{std::move(disk.error())};
    }
    auto const changed{path_ != normalized || state_.buffer_text != disk->source};
    if (!attached_) {
        state_.document_changed();
    }
    attached_ = true;
    path_ = std::move(normalized);
    auto source{disk->source};
    state_.loaded(std::move(*disk), std::move(source), changed);
    return {};
}

auto SourceSession::refresh_external_conflict() -> std::expected<void, std::string> {
    if (path_.empty() || !attached_) {
        return {};
    }
    auto disk{source_session_detail::observe(path_)};
    if (!disk) {
        state_.disk_conflict = true;
        return std::unexpected{std::move(disk.error())};
    }
    state_.observe(*disk);
    return {};
}

auto SourceSession::save() -> std::expected<void, std::string> {
    if (!attached_) {
        return std::unexpected{"Use Save Buffer As to recover the detached source buffer."};
    }
    return write(state_.buffer_text, path_, SourceOverwritePolicy::ReplaceExisting, false);
}
auto SourceSession::save_as(std::filesystem::path const& path,
                            SourceOverwritePolicy const overwrite)
    -> std::expected<void, std::string> {
    return write(state_.buffer_text, path, overwrite, false);
}
auto SourceSession::save_replacement(std::string const& source,
                                     std::string_view const synchronized_source)
    -> std::expected<void, std::string> {
    return save_replacement_as(
        source, path_, synchronized_source, SourceOverwritePolicy::ReplaceExisting);
}
auto SourceSession::save_replacement_as(std::string const& source,
                                        std::filesystem::path const& path,
                                        std::string_view const synchronized_source,
                                        SourceOverwritePolicy const overwrite)
    -> std::expected<void, std::string> {
    if (state_.unapplied(synchronized_source)) {
        return std::unexpected{"The source buffer has unapplied edits; apply or discard them "
                               "before saving canonical scene state."};
    }
    return write(source, path, overwrite, true);
}

auto SourceSession::write(std::string const& source,
                          std::filesystem::path const& path,
                          SourceOverwritePolicy const overwrite,
                          bool const replace_buffer) -> std::expected<void, std::string> {
    if (blank(source)) {
        return std::unexpected{"The S7 source buffer is empty. Use Save Canonical from Scene to "
                               "create a script from the current level."};
    }
    auto normalized{source_session_detail::normalize(path)};
    if (normalized.empty()) {
        return std::unexpected{"The source path is empty."};
    }
    if (normalized == path_) {
        // Detached recovery still protects the original file's baseline.
        auto const disk{source_session_detail::observe(path_)};
        if (!disk) {
            state_.disk_conflict = true;
            return std::unexpected{disk.error()};
        }
        state_.observe(*disk);
        if (state_.disk_conflict) {
            return std::unexpected{
                "The source file changed externally; reload it or choose another path."};
        }
    } else {
        std::error_code error;
        if (std::filesystem::exists(normalized, error) &&
            overwrite == SourceOverwritePolicy::RefuseExisting) {
            return std::unexpected{
                std::format("Source file '{}' already exists; confirm replacement before saving.",
                            path_to_utf8(normalized))};
        }
        if (error) {
            return std::unexpected{error.message()};
        }
    }
    std::ofstream file{normalized, std::ios::binary | std::ios::trunc};
    file.write(source.data(), static_cast<std::streamsize>(source.size()));
    file.close();
    if (!file) {
        return std::unexpected{
            std::format("Could not write source file '{}'.", path_to_utf8(normalized))};
    }
    auto const changed{path_ != normalized || (replace_buffer && state_.buffer_text != source)};
    path_ = std::move(normalized);
    if (replace_buffer) {
        set_buffer(source);
    }
    state_.saved(changed);
    return {};
}
auto SourceSessionState::unapplied(std::string_view const synchronized_source) const -> bool {
    return dirty() || (!buffer_text.empty() && buffer_text != synchronized_source);
}
void SourceSessionState::document_changed() {
    recovery_required = recovery_required || dirty();
    ++revision;
    scene_dirty = false;
    preview = PreviewState::None;
}
void SourceSessionState::loaded(SourceDiskState disk,
                                std::string source,
                                bool const source_changed) {
    disk_baseline = std::move(disk);
    buffer_text = std::move(source);
    saved_buffer_text = buffer_text;
    disk_conflict = false;
    recovery_required = false;
    if (source_changed) {
        ++revision;
        invalidate_preview();
    }
}
void SourceSessionState::edited(std::string source) {
    if (buffer_text == source) {
        return;
    }
    buffer_text = std::move(source);
    ++revision;
    invalidate_preview();
}
void SourceSessionState::saved(bool const source_changed) {
    disk_baseline = {.exists = true, .source = buffer_text};
    saved_buffer_text = buffer_text;
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
