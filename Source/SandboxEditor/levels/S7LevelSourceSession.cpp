#include "SandboxEditor/levels/S7LevelSourceSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"

#include <SpaceGameS7/LevelScriptCatalog.h>

#include <Containers/StringConv.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Misc/SecureHash.h>

namespace ml::editor {
auto FS7LevelSourceSession::attach(AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString> {
    auto const normalized_path{normalize_path(document.source_path)};
    if (document_.Get() == &document && path_ == normalized_path) {
        return {};
    }
    if (dirty_) {
        document_.Reset();
        external_conflict_ = true;
        return std::unexpected{
            TEXT("The source buffer has unsaved changes and was detached from the new document.")};
    }
    auto const attached{load_attached(document, normalized_path)};
    if (!attached) {
        document_.Reset();
        external_conflict_ = true;
    }
    return attached;
}

auto FS7LevelSourceSession::load(AS7LevelAuthoringDocument& document, FStringView const path)
    -> std::expected<void, FString> {
    if (dirty_) {
        return std::unexpected{TEXT("Save or discard the edited source buffer before loading.")};
    }
    auto const normalized_path{normalize_path(path)};
    auto const observed{observe_disk(normalized_path)};
    if (!observed) {
        return std::unexpected{observed.error()};
    }
    if (!observed->exists) {
        return std::unexpected{
            FString::Printf(TEXT("Could not read source file '%s'."), *normalized_path)};
    }
    return load_attached(document, path);
}

auto FS7LevelSourceSession::reload() -> std::expected<void, FString> {
    auto* const attached_document{document_.Get()};
    if (!IsValid(attached_document)) {
        return std::unexpected{TEXT("The source buffer is not attached to a level document.")};
    }
    return load_attached(*attached_document, path_);
}

auto FS7LevelSourceSession::discard_and_attach(AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString> {
    return load_attached(document, document.source_path);
}

void FS7LevelSourceSession::set_buffer(FString buffer) {
    if (buffer_ == buffer) {
        return;
    }
    buffer_ = MoveTemp(buffer);
    dirty_ = source_digest(buffer_) != disk_baseline_.digest;
    ++revision_;
}

auto FS7LevelSourceSession::save() -> std::expected<void, FString> {
    if (path_.IsEmpty()) {
        return std::unexpected{TEXT("Choose a source path before saving the source buffer.")};
    }
    return write(buffer_, path_, ES7SourceOverwritePolicy::ReplaceExisting, false);
}

auto FS7LevelSourceSession::save_as(FStringView const path,
                                    ES7SourceOverwritePolicy const overwrite)
    -> std::expected<void, FString> {
    return write(buffer_, path, overwrite, false);
}

auto FS7LevelSourceSession::save_replacement(FString source,
                                             FStringView const synchronized_source_digest)
    -> std::expected<void, FString> {
    if (has_unapplied_buffer(synchronized_source_digest)) {
        return std::unexpected{
            TEXT("The source buffer has unapplied edits; apply or discard them before saving "
                 "canonical scene state.")};
    }
    if (path_.IsEmpty()) {
        return std::unexpected{TEXT("Choose a source path before saving canonical scene state.")};
    }
    return write(source, path_, ES7SourceOverwritePolicy::ReplaceExisting, true);
}

auto FS7LevelSourceSession::save_replacement_as(FString source,
                                                FStringView const path,
                                                FStringView const synchronized_source_digest,
                                                ES7SourceOverwritePolicy const overwrite)
    -> std::expected<void, FString> {
    if (has_unapplied_buffer(synchronized_source_digest)) {
        return std::unexpected{
            TEXT("The source buffer has unapplied edits; apply or discard them before saving "
                 "canonical scene state.")};
    }
    return write(source, path, overwrite, true);
}

auto FS7LevelSourceSession::read() const -> ml::s7::FLevelDefinitionReadResult {
    if (!is_attached()) {
        return {.script_error = TEXT("The source buffer is not attached to the current level.")};
    }
    auto const source_directory{path_.IsEmpty() ? ml::s7::default_level_script_directory()
                                                : FPaths::GetPath(path_)};
    auto const library_root{FPaths::Combine(source_directory, TEXT("Libraries"))};
    return ml::s7::FLevelDefinitionReader{library_root}.read_source(buffer_);
}

auto FS7LevelSourceSession::refresh_external_conflict() -> std::expected<void, FString> {
    if (path_.IsEmpty() || !is_attached()) {
        return {};
    }
    auto const observed{observe_disk(path_)};
    if (!observed) {
        external_conflict_ = true;
        return std::unexpected{observed.error()};
    }
    external_conflict_ = *observed != disk_baseline_;
    return {};
}

auto FS7LevelSourceSession::document() const -> AS7LevelAuthoringDocument* {
    return document_.Get();
}

auto FS7LevelSourceSession::path() const -> FString const& {
    return path_;
}

auto FS7LevelSourceSession::buffer() const -> FString const& {
    return buffer_;
}

auto FS7LevelSourceSession::revision() const -> uint64 {
    return revision_;
}

auto FS7LevelSourceSession::is_attached() const -> bool {
    return document_.IsValid();
}

auto FS7LevelSourceSession::is_dirty() const -> bool {
    return dirty_;
}

auto FS7LevelSourceSession::has_external_conflict() const -> bool {
    return external_conflict_;
}

auto FS7LevelSourceSession::has_unapplied_buffer(FStringView const synchronized_source_digest) const
    -> bool {
    return dirty_ || (!buffer_.IsEmpty() && (synchronized_source_digest.IsEmpty() ||
                                             source_digest(buffer_) != synchronized_source_digest));
}

auto FS7LevelSourceSession::source_digest(FStringView const source) -> FString {
    auto const utf8{FTCHARToUTF8{source.GetData(), source.Len()}};
    uint8 digest[FSHA1::DigestSize]{};
    FSHA1::HashBuffer(utf8.Get(), utf8.Length(), digest);
    return BytesToHex(digest, UE_ARRAY_COUNT(digest));
}

auto FS7LevelSourceSession::normalize_path(FStringView const path) -> FString {
    if (path.IsEmpty()) {
        return {};
    }
    auto normalized{FPaths::ConvertRelativePathToFull(FString{path})};
    FPaths::NormalizeFilename(normalized);
    return normalized;
}

auto FS7LevelSourceSession::observe_disk(FStringView const path)
    -> std::expected<FDiskState, FString> {
    auto const owned_path{FString{path}};
    if (!IFileManager::Get().FileExists(*owned_path)) {
        return FDiskState{};
    }
    FString source;
    if (!FFileHelper::LoadFileToString(source, *owned_path)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not read source file '%s'."), *owned_path)};
    }
    return FDiskState{.exists = true, .digest = source_digest(source)};
}

auto FS7LevelSourceSession::load_attached(AS7LevelAuthoringDocument& document,
                                          FStringView const path) -> std::expected<void, FString> {
    auto const normalized_path{normalize_path(path)};
    FString loaded_buffer;
    FDiskState loaded_disk;
    if (!normalized_path.IsEmpty()) {
        auto const observed{observe_disk(normalized_path)};
        if (!observed) {
            return std::unexpected{observed.error()};
        }
        if (observed->exists && !FFileHelper::LoadFileToString(loaded_buffer, *normalized_path)) {
            return std::unexpected{
                FString::Printf(TEXT("Could not read source file '%s'."), *normalized_path)};
        }
        loaded_disk = observed->exists
                        ? *observed
                        : FDiskState{.exists = false, .digest = source_digest(loaded_buffer)};
    } else {
        loaded_disk.digest = source_digest(loaded_buffer);
    }

    auto const source_changed{path_ != normalized_path || buffer_ != loaded_buffer};
    document_ = &document;
    path_ = normalized_path;
    buffer_ = MoveTemp(loaded_buffer);
    disk_baseline_ = MoveTemp(loaded_disk);
    dirty_ = false;
    external_conflict_ = false;
    if (source_changed) {
        ++revision_;
    }
    return {};
}

auto FS7LevelSourceSession::verify_disk_baseline() -> std::expected<void, FString> {
    auto const observed{observe_disk(path_)};
    if (!observed) {
        external_conflict_ = true;
        return std::unexpected{observed.error()};
    }
    if (*observed != disk_baseline_) {
        external_conflict_ = true;
        return std::unexpected{
            TEXT("The source file changed externally; reload it or choose another path.")};
    }
    return {};
}

auto FS7LevelSourceSession::write(FString const& source,
                                  FStringView const target_path,
                                  ES7SourceOverwritePolicy const overwrite,
                                  bool const replace_buffer) -> std::expected<void, FString> {
    auto const normalized_path{normalize_path(target_path)};
    if (normalized_path.IsEmpty()) {
        return std::unexpected{TEXT("The source path is empty.")};
    }

    auto const same_path{normalized_path == path_};
    if (same_path) {
        auto const verified{verify_disk_baseline()};
        if (!verified) {
            return verified;
        }
    } else if (IFileManager::Get().FileExists(*normalized_path) &&
               overwrite == ES7SourceOverwritePolicy::RefuseExisting) {
        return std::unexpected{FString::Printf(
            TEXT("Source file '%s' already exists; confirm replacement before saving."),
            *normalized_path)};
    }

    if (!FFileHelper::SaveStringToFile(
            source, *normalized_path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        return std::unexpected{
            FString::Printf(TEXT("Could not write source file '%s'."), *normalized_path)};
    }

    auto const source_changed{path_ != normalized_path || (replace_buffer && buffer_ != source)};
    path_ = normalized_path;
    if (replace_buffer) {
        buffer_ = source;
    }
    disk_baseline_ = {.exists = true, .digest = source_digest(source)};
    dirty_ = replace_buffer ? false : source_digest(buffer_) != disk_baseline_.digest;
    external_conflict_ = false;
    if (source_changed) {
        ++revision_;
    }
    return {};
}
}
