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
    if (attached_ || (state_.dirty() && !state_.recovery_required)) {
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
        return std::unexpected{
            "The source buffer is detached. Copy any edits before discarding it."};
    }
    auto const& source{state_.buffer_text};
    if (blank(source)) {
        return std::unexpected{"The S7 source buffer is empty."};
    }
    if (path_.empty()) {
        return std::unexpected{"The source path is empty."};
    }

    auto const observed{refresh_external_conflict()};
    if (!observed) {
        return observed;
    }
    if (state_.disk_conflict) {
        return std::unexpected{
            "The source file changed externally; copy any edits before reloading it."};
    }

    std::ofstream file{path_, std::ios::binary | std::ios::trunc};
    file.write(source.data(), static_cast<std::streamsize>(source.size()));
    file.close();
    if (!file) {
        return std::unexpected{
            std::format("Could not write source file '{}'.", path_to_utf8(path_))};
    }

    state_.saved();
    return {};
}

void SourceSessionState::document_changed() {
    recovery_required = recovery_required || dirty();
    ++revision;
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
    }
}
void SourceSessionState::edited(std::string source) {
    if (buffer_text == source) {
        return;
    }
    buffer_text = std::move(source);
    ++revision;
}
void SourceSessionState::saved() {
    disk_baseline = {.exists = true, .source = buffer_text};
    saved_buffer_text = buffer_text;
    disk_conflict = false;
}
void SourceSessionState::observe(SourceDiskState const& disk) {
    disk_conflict = disk != disk_baseline;
}

}
