#include "SandboxEditor/levels/S7LevelSourceSession.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include <ioj/files.h>
#include <SpaceGameS7/level_script_catalog.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj::editor {
namespace s7_source_session_detail {
auto text(FStringView const source) -> std::string {
    auto const utf8{FTCHARToUTF8{source.GetData(), source.Len()}};
    return {utf8.Get(), static_cast<std::size_t>(utf8.Length())};
}
auto path(FStringView const source) -> std::filesystem::path {
    return std::filesystem::path{*FString{source}};
}
auto result(std::expected<void, std::string> value) -> std::expected<void, FString> {
    return value.transform_error([](auto const& error) { return to_fstring(error); });
}
}
using namespace s7_source_session_detail;

auto FS7LevelSourceSession::attach(AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString> {
    if (document_.Get() != &document) {
        detach();
    }
    auto attached{session_.attach(s7_source_session_detail::path(document.source_path))};
    document_ = attached ? &document : nullptr;
    return result(std::move(attached));
}
auto FS7LevelSourceSession::load(AS7LevelAuthoringDocument& document, FStringView const source_path)
    -> std::expected<void, FString> {
    if (document_.Get() != &document) {
        detach();
    }
    auto loaded{session_.load(s7_source_session_detail::path(source_path))};
    if (loaded) {
        document_ = &document;
    }
    return result(std::move(loaded));
}
auto FS7LevelSourceSession::reload() -> std::expected<void, FString> {
    if (!document_.IsValid()) {
        session_.detach();
    }
    return result(session_.reload());
}
auto FS7LevelSourceSession::discard_and_attach(AS7LevelAuthoringDocument& document)
    -> std::expected<void, FString> {
    if (document_.Get() != &document) {
        detach();
    }
    auto attached{
        session_.discard_and_attach(s7_source_session_detail::path(document.source_path))};
    if (attached) {
        document_ = &document;
    }
    return result(std::move(attached));
}
void FS7LevelSourceSession::detach() {
    session_.detach();
    document_.Reset();
}
void FS7LevelSourceSession::set_buffer(FString source) {
    session_.set_buffer(text(source));
}
auto FS7LevelSourceSession::save() -> std::expected<void, FString> {
    if (!document_.IsValid()) {
        session_.detach();
    }
    return result(session_.save());
}
auto FS7LevelSourceSession::save_as(FStringView const source_path,
                                    ES7SourceOverwritePolicy const overwrite)
    -> std::expected<void, FString> {
    if (!document_.IsValid()) {
        session_.detach();
    }
    return result(session_.save_as(s7_source_session_detail::path(source_path), overwrite));
}
auto FS7LevelSourceSession::save_replacement(FString source, FStringView const synchronized_source)
    -> std::expected<void, FString> {
    return result(session_.save_replacement(text(source), text(synchronized_source)));
}
auto FS7LevelSourceSession::save_replacement_as(FString source,
                                                FStringView const source_path,
                                                FStringView const synchronized_source,
                                                ES7SourceOverwritePolicy const overwrite)
    -> std::expected<void, FString> {
    return result(session_.save_replacement_as(text(source),
                                               s7_source_session_detail::path(source_path),
                                               text(synchronized_source),
                                               overwrite));
}
auto FS7LevelSourceSession::refresh_external_conflict() -> std::expected<void, FString> {
    if (!document_.IsValid()) {
        session_.detach();
    }
    return result(session_.refresh_external_conflict());
}
auto FS7LevelSourceSession::document() const -> AS7LevelAuthoringDocument* {
    return document_.Get();
}
auto FS7LevelSourceSession::path() const -> FString {
    return FString{session_.path().generic_wstring().c_str()};
}
auto FS7LevelSourceSession::buffer() const -> FString {
    return to_fstring(session_.buffer());
}
auto FS7LevelSourceSession::revision() const -> uint64 {
    return session_.state().revision;
}
auto FS7LevelSourceSession::is_attached() const -> bool {
    return document_.IsValid() && session_.is_attached();
}
auto FS7LevelSourceSession::is_dirty() const -> bool {
    return session_.state().dirty();
}
auto FS7LevelSourceSession::has_external_conflict() const -> bool {
    return session_.state().disk_conflict;
}
auto FS7LevelSourceSession::has_unapplied_buffer(FStringView const synchronized_source) const
    -> bool {
    return session_.state().unapplied(text(synchronized_source));
}

auto FS7LevelSourceSession::read() const
    -> std::expected<levels::LevelDefinition, levels::Diagnostics> {
    if (!is_attached()) {
        return std::unexpected{
            levels::Diagnostics{{levels::DiagnosticCode::ExpectedRecord,
                                 "$",
                                 "The source buffer is not attached to the current level."}}};
    }
    auto const root{
        session_.path().empty()
            ? s7_source_session_detail::path(levels::authoring::default_level_script_directory())
            : session_.path().parent_path()};
    return levels::authoring::DefinitionReader{root / "Libraries"}.read_level_source(
        session_.buffer());
}
}
