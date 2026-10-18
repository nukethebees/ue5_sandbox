#include <ioj/levels/authoring/source_session.h>

#include <ioj/ascii.h>
#include <ioj/files.h>

#include <array>
#include <bit>
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
    if (bytes->starts_with("\xef\xbb\xbf")) {
        bytes->erase(0, 3);
    } else if (bytes->starts_with("\xff\xfe") || bytes->starts_with("\xfe\xff")) {
        auto const little{static_cast<unsigned char>((*bytes)[0]) == 0xff};
        std::string utf8;
        auto unit{[&](std::size_t const i) -> std::uint32_t {
            auto const a{static_cast<unsigned char>((*bytes)[i])};
            auto const b{static_cast<unsigned char>((*bytes)[i + 1])};
            return little ? a | (b << 8) : (a << 8) | b;
        }};
        for (std::size_t i{2}; i + 1 < bytes->size(); i += 2) {
            auto code{unit(i)};
            if (code >= 0xd800 && code <= 0xdbff && i + 3 < bytes->size()) {
                auto const low{unit(i + 2)};
                if (low >= 0xdc00 && low <= 0xdfff) {
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    i += 2;
                }
            }
            if (code >= 0xd800 && code <= 0xdfff) {
                code = 0xfffd;
            }
            if (code < 0x80) {
                utf8.push_back(static_cast<char>(code));
            } else {
                if (code >= 0x10000) {
                    utf8.push_back(static_cast<char>(0xf0 | (code >> 18)));
                    utf8.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
                } else if (code >= 0x800) {
                    utf8.push_back(static_cast<char>(0xe0 | (code >> 12)));
                } else {
                    utf8.push_back(static_cast<char>(0xc0 | (code >> 6)));
                }
                if (code >= 0x800) {
                    utf8.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                }
                utf8.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
        }
        return utf8;
    }
    return std::move(*bytes);
}

auto observe(std::filesystem::path const& path, std::string* const output = nullptr)
    -> std::expected<SourceDiskState, std::string> {
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
    auto result{SourceDiskState{true, SourceSession::source_digest(*source)}};
    if (output) {
        *output = std::move(*source);
    }
    return result;
}
}

auto SourceSession::source_digest(std::string_view const source) -> std::string {
    // Preserve the SHA-1 digests stored in existing Unreal authoring documents.
    std::array<std::uint32_t, 5> hash{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    auto const padded_size{((source.size() + 9 + 63) / 64) * 64};
    auto const bit_count{static_cast<std::uint64_t>(source.size()) * 8};
    for (std::size_t offset{}; offset < padded_size; offset += 64) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t i{}; i < 64; ++i) {
            auto const index{offset + i};
            auto const byte{
                index < source.size()    ? static_cast<std::uint8_t>(source[index])
                : index == source.size() ? std::uint8_t{0x80}
                : index >= padded_size - 8
                    ? static_cast<std::uint8_t>(bit_count >> ((padded_size - 1 - index) * 8))
                    : std::uint8_t{0}};
            words[i / 4] |= static_cast<std::uint32_t>(byte) << ((3 - i % 4) * 8);
        }
        for (std::size_t i{16}; i < words.size(); ++i) {
            words[i] = std::rotl(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
        }
        auto a{hash[0]}, b{hash[1]}, c{hash[2]}, d{hash[3]}, e{hash[4]};
        for (std::size_t i{}; i < words.size(); ++i) {
            auto const f{i < 20   ? (b & c) | (~b & d)
                         : i < 40 ? b ^ c ^ d
                         : i < 60 ? (b & c) | (b & d) | (c & d)
                                  : b ^ c ^ d};
            auto const k{i < 20   ? 0x5a827999u
                         : i < 40 ? 0x6ed9eba1u
                         : i < 60 ? 0x8f1bbcdcu
                                  : 0xca62c1d6u};
            auto const next{std::rotl(a, 5) + f + e + k + words[i]};
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = next;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }
    return std::format(
        "{:08X}{:08X}{:08X}{:08X}{:08X}", hash[0], hash[1], hash[2], hash[3], hash[4]);
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
    if (source == buffer_) {
        return;
    }
    buffer_ = std::move(source);
    state_.edited(source_digest(buffer_), buffer_.empty());
}

auto SourceSession::load_attached(std::filesystem::path const& path)
    -> std::expected<void, std::string> {
    auto normalized{source_session_detail::normalize(path)};
    std::string source;
    auto disk{source_session_detail::observe(normalized, &source)};
    if (!disk) {
        return std::unexpected{std::move(disk.error())};
    }
    auto const changed{path_ != normalized || buffer_ != source};
    if (!attached_) {
        state_.document_changed();
    }
    attached_ = true;
    path_ = std::move(normalized);
    buffer_ = std::move(source);
    state_.loaded(std::move(*disk), source_digest(buffer_), buffer_.empty(), changed);
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
    return write(buffer_, path_, SourceOverwritePolicy::ReplaceExisting, false);
}
auto SourceSession::save_as(std::filesystem::path const& path,
                            SourceOverwritePolicy const overwrite)
    -> std::expected<void, std::string> {
    return write(buffer_, path, overwrite, false);
}
auto SourceSession::save_replacement(std::string source, std::string_view const synchronized_digest)
    -> std::expected<void, std::string> {
    return save_replacement_as(
        std::move(source), path_, synchronized_digest, SourceOverwritePolicy::ReplaceExisting);
}
auto SourceSession::save_replacement_as(std::string source,
                                        std::filesystem::path const& path,
                                        std::string_view const synchronized_digest,
                                        SourceOverwritePolicy const overwrite)
    -> std::expected<void, std::string> {
    if (state_.unapplied(synchronized_digest)) {
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
    auto const changed{path_ != normalized || (replace_buffer && buffer_ != source)};
    path_ = std::move(normalized);
    if (replace_buffer) {
        set_buffer(source);
    }
    state_.saved(state_.buffer_digest, changed);
    return {};
}
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
